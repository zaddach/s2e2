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

#include "cxx11-compat.h"

#include <string>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Function.h>

extern "C" {

#include <stdint.h>
#include <inttypes.h>

#include "qemu-common.h"
#include "exec/cpu-common.h"
#include "exec/exec-all.h"
#include "tcg.h"
#include "tcg-op.h"
#include "tcg-plugin.h"
#include "tcg-llvm.h"



static void tpi_after_gen_tb(const TCGPluginInterface *tpi)
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

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION(*tpi);
    tpi->after_gen_tb = tpi_after_gen_tb;
	
    tcg_llvm_ctx = tcg_llvm_initialize();
}

} /* extern "C" */
