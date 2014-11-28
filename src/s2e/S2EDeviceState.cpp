/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

#include <tcgplugin/cxx11-compat.h>

#include <llvm/Support/CommandLine.h>

extern "C" {
#include <qemu-common.h>
#include <exec/cpu-common.h>
#include <block/block.h>
#include <qapi-types.h>
#include <migration/qemu-file.h>
#include <tcg-plugin.h>

void vm_stop(int reason);
void vm_start(void);
}

#include <s2e/s2e_block.h>

#include <iostream>
#include <sstream>
#include <s2e/Utils.h>
#include <s2e/S2E.h>
#include <s2e/s2e_qemu.h>
#include "s2e/S2EDeviceState.h"
#include "s2e/S2EExecutionState.h"

namespace {
    //Force writes to disk to be persistent (and disable copy on write)
    llvm::cl::opt<bool>
    PersistentDiskWrites("s2e-persistent-disk-writes",
                     llvm::cl::init(false));
    //Share device state between states
    llvm::cl::opt<std::string>
    SharedDevices("s2e-shared-devices",
        llvm::cl::desc("Comma-separated list of devices to be shared between states."),
        llvm::cl::init(""));
}


using namespace s2e;
using namespace std;
using namespace klee;

std::vector<struct SaveStateEntry *> S2EDeviceState::s_devices;
llvm::SmallVector<struct BlockDriverState*, 5> S2EDeviceState::s_blockDevices;

uint8_t *S2EDeviceState::s_tempStateBuffer = NULL;
unsigned S2EDeviceState::s_tempStateSize = 0;
unsigned S2EDeviceState::s_finalStateSize = 0;

bool S2EDeviceState::s_devicesInited=false;

extern "C" {

void s2e_init_device_state(S2EExecutionState *s)
{
    s->getDeviceState()->initDeviceState();
}

}

int S2EDeviceState::getBufferInternal(void *opaque, uint8_t *buf, int64_t pos, int size) {
    assert(opaque);
    return static_cast<S2EDeviceState *>(opaque)->getBuffer(buf, pos, size);
}

int S2EDeviceState::putBufferInternal(void *opaque, uint8_t const *buf, int64_t pos, int size) {
    assert(opaque);
    return static_cast<S2EDeviceState *>(opaque)->putBuffer(buf, pos, size);
}

S2EDeviceState::S2EDeviceState(const S2EDeviceState &state)
    : m_deviceState(state.m_deviceState),
      m_stateBuffer(nullptr),
      m_memFile(nullptr),
      m_fileOps(nullptr)
{
    assert( state.m_stateBuffer && s_finalStateSize > 0);

    m_stateBuffer = new uint8_t[s_finalStateSize];
    assert(m_stateBuffer);
    m_fileOps = new QEMUFileOps;
    assert(m_fileOps);

    memcpy(m_stateBuffer, state.m_stateBuffer, s_finalStateSize);

    m_fileOps->get_buffer = &S2EDeviceState::getBufferInternal;
    m_fileOps->put_buffer = &S2EDeviceState::putBufferInternal;
    m_memFile = qemu_fopen_ops(this, m_fileOps);
}

S2EDeviceState::S2EDeviceState(klee::ExecutionState *state)
    : m_deviceState(state),
      m_stateBuffer(nullptr),
      m_memFile(nullptr),
      m_fileOps(nullptr)
{
}

S2EDeviceState::~S2EDeviceState()
{
    if (m_stateBuffer) {
        free(m_stateBuffer);
    }

    if (m_memFile)  {
        qemu_fclose(m_memFile);
    }
}

void S2EDeviceState::initDeviceState()
{
    assert(!m_stateBuffer);
    assert(!m_memFile);
    
    assert(!s_devicesInited);

    std::set<std::string> ignoreList;
    std::stringstream ss(SharedDevices);
    std::string s;
    while (getline(ss, s, ',')) {
        ignoreList.insert(s);
    }

    //S2E manages the memory and disc state on its own
    ignoreList.insert("ram");
    ignoreList.insert("block");

    //The CPU is taken care of be the S2E executor
    //XXX: What about watchpoints and stuff like that?
#ifndef TARGET_ARM
    //TODO: check what to do for the ARM port
    ignoreList.insert("cpu");
#endif

    g_s2e->getMessagesStream() << "Initing initial device state." << '\n';

    if (!s_devicesInited) {
        g_s2e->getDebugStream() << "Looking for relevant virtual devices...";

        //Register all active devices detected by QEMU
        for(struct SaveStateEntry *se = tcgplugin_savevm_handlers_first();
            se != nullptr; se = tcgplugin_savevm_handlers_next(se)) {
                std::string deviceId(tcgplugin_savevm_handler_get_idstr(se));
                if (!ignoreList.count(deviceId)) {
                    g_s2e->getDebugStream() << "   Registering device " << deviceId << '\n';
                    s_devices.push_back(se);
                } else {
                    g_s2e->getDebugStream() << "   Shared device " << deviceId << '\n';
                }
        }
        s_devicesInited = true;
    }

    if (!PersistentDiskWrites) {
        g_s2e->getMessagesStream() <<
                "WARNING!!! All writes to disk will be lost after shutdown." << '\n';
        __hook_bdrv_read = s2e_bdrv_read;
        __hook_bdrv_write = s2e_bdrv_write;
    }else {
        g_s2e->getMessagesStream() <<
                "WARNING!!! All disk writes will be SHARED across states! BEWARE OF CORRUPTION!" << '\n';
    }
}

void S2EDeviceState::saveDeviceState()
{
//    qemu_make_readable(s_memFile);

    //DPRINTF("Saving device state %p\n", this);
    /* Iterate through all device descritors and call
    * their snapshot function */
	for ( struct SaveStateEntry *se : s_devices ) {
        tcgplugin_vmstate_save(m_memFile, se);
    }
    //DPRINTF("\n");
    qemu_fflush(m_memFile);
    initFirstSnapshot();
}

void S2EDeviceState::initFirstSnapshot()
{
    if (s_finalStateSize) {
        return;
    }

    assert(!m_stateBuffer && s_tempStateBuffer && s_tempStateSize);
    m_stateBuffer = (uint8_t*) malloc(s_tempStateSize);
    if (!m_stateBuffer) {
        llvm::errs() << "S2EDeviceState: could not allocate memory\n";
        exit(-1);
    }

    memcpy(m_stateBuffer, s_tempStateBuffer, s_tempStateSize);
    s_finalStateSize = s_tempStateSize;
    free(s_tempStateBuffer);
    s_tempStateBuffer = NULL;
    s_tempStateSize = 0;
}

void S2EDeviceState::restoreDeviceState()
{
    assert(s_finalStateSize && m_stateBuffer);

  //  qemu_make_readable(m_memFile);
    //DPRINTF("Restoring device state %p\n", this);
    for ( struct SaveStateEntry *se : s_devices )  {
        //DPRINTF("%s ", s2e_qemu_get_se_idstr(se));  
    	//TODO: find appropriate version id
        tcgplugin_vmstate_load(m_memFile, se, 0);
    }
    //DPRINTF("\n");
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

void S2EDeviceState::allocateBuffer(unsigned int Sz)
{
    if (Sz < s_tempStateSize) {
        return;
    }

    /* Need to expand the buffer */
    s_tempStateSize = Sz;
    s_tempStateBuffer = (uint8_t*) realloc(s_tempStateBuffer, s_tempStateSize);
    if (!s_tempStateBuffer) {
        cerr << "Cannot reallocate memory for device state snapshot" << endl;
        exit(-1);
    }
}

int S2EDeviceState::putBuffer(const uint8_t *buf, int64_t pos, int size)
{
    uint8_t *dest;

    if (!m_stateBuffer) {
        allocateBuffer(pos + size);
        dest = &s_tempStateBuffer[pos];
    } else {
        dest = &m_stateBuffer[pos];
    }

    memcpy(dest, buf, size);
    return size;
}

int S2EDeviceState::getBuffer(uint8_t *buf, int64_t pos, int size)
{
    assert(m_stateBuffer);
    int toCopy = pos + size <= s_finalStateSize ? size : s_finalStateSize - pos;
    memcpy(buf, &m_stateBuffer[pos], toCopy);
    return size;


    //memcpy(buf, &m_state[pos], toCopy);
    //return toCopy;
}


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

unsigned S2EDeviceState::getBlockDeviceId(struct BlockDriverState* dev)
{
    unsigned i = 0;
    for ( auto blkdev : s_blockDevices )  {
        if (blkdev == dev) {
            return i;
        }
        ++i;
    }
    s_blockDevices.push_back(dev);
    return i;
}

uint64_t S2EDeviceState::getBlockDeviceStart(struct BlockDriverState* dev)
{
    unsigned id = getBlockDeviceId(dev);
    return id * BLOCK_DEV_AS;
}

/* Return 0 upon success */
int S2EDeviceState::writeSector(struct BlockDriverState *bs, int64_t sector, const uint8_t *buf, int nb_sectors)
{
    uint64_t bstart = getBlockDeviceStart(bs);

    while (nb_sectors > 0) {
        uintptr_t address = (uintptr_t) bstart + sector * SECTOR_SIZE;
        ObjectPair op = m_deviceState.findObject(address);
        if (op.first == NULL) {
            MemoryObject *mo = new MemoryObject(address, SECTOR_SIZE, false, false, false, NULL);
            ObjectState *os = new ObjectState(mo);
            m_deviceState.bindObject(mo, os);
            op.first = mo;
            op.second = os;
        }

        ObjectState *os = m_deviceState.getWriteable(op.first, op.second);
        memcpy(os->getConcreteStore(false), buf, SECTOR_SIZE);
        buf += SECTOR_SIZE;
        --nb_sectors;
        ++sector;
    }

    return 0;
}

/* Return the number of sectors that could be read from the local store */
int S2EDeviceState::readSector(struct BlockDriverState *bs, int64_t sector, uint8_t *buf, int nb_sectors)
{
    int readCount = 0;

    uint64_t bstart = getBlockDeviceStart(bs);

    while (nb_sectors > 0) {
        uintptr_t address = (uintptr_t) bstart + sector * SECTOR_SIZE;
        ObjectPair op = m_deviceState.findObject(address);
        if (!op.first) {
            return readCount;
        }

        memcpy(buf, op.second->getConcreteStore(false), SECTOR_SIZE);
        buf += SECTOR_SIZE;
        ++readCount;
        --nb_sectors;
        ++sector;
    }

    return readCount;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/**
 *  Functions facing QEMU. They simply forward the call to the right
 *  device state.
 */

extern "C" {

int s2e_bdrv_read(struct BlockDriverState *bs, int64_t sector_num,
                  uint8_t *buf, int nb_sectors)
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    return devState->readSector(bs, sector_num, buf, nb_sectors);
}

int s2e_bdrv_write(BlockDriverState *bs, int64_t sector_num,
                   const uint8_t *buf, int nb_sectors
                   )
{
    S2EDeviceState *devState = g_s2e_state->getDeviceState();
    
    return devState->writeSector(bs, sector_num, buf, nb_sectors);

}

void s2e_bdrv_fail(void)
{
    fprintf(stderr,
            "\n\033[31m========================================================================\n"
            "You are using a disk image format not compatible with symbolic execution\n"
            "(qcow2, etc.).\n\n"
            "Please use the S2E image format for your VM when running in S2E mode.\n"
            "The S2E format is identical to the RAW format, except that the filename\n"
            "of the image ends with the .s2e extension and snapshots are saved in a\n"
            "separate file, in the same folder as the base image.\n"
            "The S2E image and snapshots are always read-only, multiple S2E instances\n"
            "can use them at the same time.\n\n"
            "Refer to the S2E documentation for more details.\n"
            "========================================================================\033[0m\n"
            );
    exit(-1);
}

}
