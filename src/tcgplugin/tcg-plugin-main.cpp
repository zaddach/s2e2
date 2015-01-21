/*
 * TCG plugin for QEMU: Translate TCG code to LLVM.
 *
 * Copyright (C) 2014 Jonas Zaddach <zaddach@eurecom.fr>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tcgplugin/cxx11-compat.h"

#include <string>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Function.h>

#include <fstream>

//File generated by protobuf interface compiler
#include "trace_format.pb.h"

extern "C" {

#include <stdint.h>
#include <inttypes.h>

#include "qemu-common.h"
#include "exec/cpu-common.h"
#include "exec/exec-all.h"
#include "tcg.h"
#include "tcg-op.h"
#include "tcg-plugin.h"
#include "tcgplugin/tcg-llvm.h"


static std::ofstream trace_stream;

static void init_trace()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    trace_stream.open("/tmp/trace.protobuf", std::ios_base::out | std::ios_base::binary);
}


static void after_gen_tb(const TCGPluginInterface *tpi)
{
    tcg_llvm_gen_code(tcg_llvm_ctx, tpi->tcg_ctx, tpi->tb);

    assert(tpi->tb->tcg_plugin_opaque);

    std::string fcnString;
    llvm::raw_string_ostream ss(fcnString);

    llvm::Function *function = static_cast<TCGPluginTBData *>(tpi->tb->tcg_plugin_opaque)->llvm_function;
    ss << *function;
    ss.flush();
    fprintf(tpi->output, "%s", fcnString.c_str());
}

static void monitor_memory_access(const TCGPluginInterface *tpi, uint64_t address, uint32_t mmu_idx, uint64_t val, uint32_t memop_size, enum MemoryAccessType type)
{
    trace::TraceEvent trace_entry;
    trace::MemoryAccess memory_access;


    memory_access.set_address(address);
    memory_access.set_mmu_index(mmu_idx);
    memory_access.set_value(val);
    switch (memop_size & MO_SIZE) {
    case MO_8:
        memory_access.set_size(trace::MemoryAccess::INT_8); break;
    case MO_16:
        memory_access.set_size(trace::MemoryAccess::INT_16); break;
    case MO_32:
        memory_access.set_size(trace::MemoryAccess::INT_32); break;
    case MO_64:
        memory_access.set_size(trace::MemoryAccess::INT_64); break;
    default:
        assert(0);
    }
    switch (type) {
    case MEMORY_ACCESS_TYPE_READ:
        memory_access.set_type(trace::MemoryAccess::READ); break;
    case MEMORY_ACCESS_TYPE_WRITE:
        memory_access.set_type(trace::MemoryAccess::WRITE); break;
    default:
        assert(0);
    }

    trace_entry.set_allocated_memory_access(&memory_access);

    if (!trace_entry.SerializeToOstream(&trace_stream)) {
        //TODO: Error serializing
    }

    trace_entry.release_memory_access();
}

static void qemu_exit(const TCGPluginInterface *tpi)
{
    trace_stream.close();
}

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION(*tpi);
    tpi->after_gen_tb = after_gen_tb;
    tpi->monitor_memory_access = monitor_memory_access;
    tpi->exit = qemu_exit;

    tpi->do_monitor_memory_access = true;
	
    tcg_llvm_ctx = tcg_llvm_initialize();
    init_trace();
}

} /* extern "C" */
