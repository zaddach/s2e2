/*
 * TCG plugin for QEMU: simulate a IO memory mapped device (mainly
 *                      interesting to prototype things in user-mode).
 *
 * Copyright (C) 2011 STMicroelectronics
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

/* You can test this plugin in user-mode with the following code:
 *
 * int main(void)
 * {
 * 	char *device = mmap(0xCAFE0000, 1024, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
 * 	return printf("printf(%p): %s\n", device, device);
 * }
 */

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
}

void tpi_init(TCGPluginInterface *tpi)
{
    TPI_INIT_VERSION(*tpi);
    tpi->after_gen_tb = tpi_after_gen_tb;
	
    tcg_llvm_ctx = tcg_llvm_initialize();
}

} /* extern "C" */
