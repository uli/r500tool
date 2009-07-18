/*
 * darwinio - userspace part for DirectIO
 *
 * Copyright © 2008 coresystems GmbH <info@coresystems.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <AvailabilityMacros.h>
#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <errno.h>

/* define WANT_OLD_API for support of OSX 10.4 and earlier */
#undef WANT_OLD_API

/* define NO_FORCE_ROOT to prevent DirectIO iopl from failing when being called
 * by a non-root user. This should be handled in the kernel driver rather than
 * in the userspace library. This implementation is a security hole. Patches
 * welcome.
 */
#define NO_FORCE_ROOT

/* define DEBUG to print Framework debugging information */
#undef DEBUG

#define     err_get_system(err) (((err)>>26)&0x3f)
#define     err_get_sub(err)    (((err)>>14)&0xfff)
#define     err_get_code(err)   ((err)&0x3fff)

enum {
	kReadIO,
	kWriteIO,
	kPrepareMap,
	kReadMSR,
	kWriteMSR,
	kNumberOfMethods
};

typedef struct {
	UInt16 offset;
	UInt8 width;
	UInt32 data;
} iomem_t;

typedef struct {
	UInt32 addr;
	UInt32 size;
} map_t;

typedef struct {
	UInt32	index;
	UInt32	hi;
	UInt32	lo;
} msrcmd_t;

typedef struct { uint32_t hi, lo; } msr_t;

static io_connect_t connect = -1;
static io_service_t iokit_uc;

static int darwin_init(void)
{
	kern_return_t err;

#ifndef NO_FORCE_ROOT
#warning forcing root
	if (getuid() != 0) {
		/* Fun's reserved for root */
		errno = EPERM;
		return -1;
	}
#endif

	/* Get the DirectIO driver service */
	iokit_uc = IOServiceGetMatchingService(kIOMasterPortDefault,
					IOServiceMatching("DirectIOService"));

	if (!iokit_uc) {
		printf("DirectIO.kext not loaded.\n");
		errno = ENOSYS;
		return -1;
	}

	/* Create an instance */
	err = IOServiceOpen(iokit_uc, mach_task_self(), 0, &connect);

	/* Should not go further if error with service open */
	if (err != KERN_SUCCESS) {
		printf("Could not create DirectIO instance.");
		errno = ENOSYS;
		return -1;
	}

	return 0;
}

static void darwin_cleanup(void)
{
	IOServiceClose(connect);
}

static int darwin_ioread(int pos, unsigned char * buf, int len)
{

	kern_return_t err;
	IOByteCount dataInLen = sizeof(iomem_t);
	IOByteCount dataOutLen = sizeof(iomem_t);
	iomem_t in;
	iomem_t out;
	UInt32 tmpdata;

	in.width = len;
	in.offset = pos;

	if (len > 4)
		return 1;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kReadIO, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kReadIO, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return 1;

	tmpdata = out.data;

	switch (len) {
	case 1:
		memcpy(buf, &tmpdata, 1);
		break;
	case 2:
		memcpy(buf, &tmpdata, 2);
		break;
	case 4:
		memcpy(buf, &tmpdata, 4);
		break;
	}

	return 0;
}

static int darwin_iowrite(int pos, unsigned char * buf, int len)
{
	kern_return_t err;
	IOByteCount dataInLen = sizeof(iomem_t);
	IOByteCount dataOutLen = sizeof(iomem_t);
	iomem_t in;
	iomem_t out;

	in.width = len;
	in.offset = pos;
	memcpy(&in.data, buf, len);

	if (len > 4)
		return 1;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kWriteIO, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kWriteIO, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return 1;

	return 0;
}


/* Compatibility interface */

unsigned char inb(unsigned short addr)
{
	unsigned char ret;
	darwin_ioread(addr, &ret, 1);
	return ret;
}

unsigned short inw(unsigned short addr)
{
	unsigned short ret;
	darwin_ioread(addr, (unsigned char *)&ret, 2);
	return ret;
}

unsigned int inl(unsigned short addr)
{
	unsigned int ret;
	darwin_ioread(addr, (unsigned char *)&ret, 4);
	return ret;
}

void outb(unsigned char val, unsigned short addr)
{
	darwin_iowrite(addr, &val, 1);
}

void outw(unsigned short val, unsigned short addr)
{
	darwin_iowrite(addr, (unsigned char *)&val, 2);
}

void outl(unsigned int val, unsigned short addr)
{
	darwin_iowrite(addr, (unsigned char *)&val, 4);
}

int iopl(int level __attribute__((unused)))
{
	atexit(darwin_cleanup);
	return darwin_init();
}

void *map_physical(unsigned long phys_addr, int len)
{
	kern_return_t err;
        vm_address_t addr;
        vm_size_t size;
	IOByteCount dataInLen = sizeof(map_t);
	IOByteCount dataOutLen = sizeof(map_t);
	map_t in, out;

	in.addr = phys_addr;
	in.size = len;

#ifdef DEBUG
	printf("map_phys: phys %08lx, %08x\n", phys_addr, len);
#endif

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kPrepareMap, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kPrepareMap, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS) {
		printf("\nError(kPrepareMap): system 0x%x subsystem 0x%x code 0x%x ",
				err_get_system(err), err_get_sub(err), err_get_code(err));

		printf("physical %p[0x%x]\n", (void *)phys_addr, len);

		switch (err_get_code(err)) {
		case 0x2c2: printf("Invalid Argument.\n"); break;
		case 0x2cd: printf("Device not Open.\n"); break;
		}

		return NULL;
	}

        err = IOConnectMapMemory(connect, 0, mach_task_self(),
			&addr, &size, kIOMapAnywhere | kIOMapInhibitCache);

	/* Now this is odd; The above connect seems to be unfinished at the
	 * time the function returns. So wait a little bit, or the calling
	 * program will just segfault. Bummer. Who knows a better solution?
	 */
	usleep(1000);

	if (err != KERN_SUCCESS) {
		printf("\nError(IOConnectMapMemory): system 0x%x subsystem 0x%x code 0x%x ",
				err_get_system(err), err_get_sub(err), err_get_code(err));

		printf("physical %p[0x%x]\n", (void *)phys_addr, len);

		switch (err_get_code(err)) {
		case 0x2c2: printf("Invalid Argument.\n"); break;
		case 0x2cd: printf("Device not Open.\n"); break;
		}

		return NULL;
	}

#ifdef DEBUG
	printf("map_phys: virt %08x, %08x\n", addr, size);
#endif

        return (void *)addr;
}

void unmap_physical(void *virt_addr __attribute__((unused)), int len __attribute__((unused)))
{
	// Nut'n Honey
}

msr_t rdmsr(int addr)
{
	kern_return_t err;
	IOByteCount dataInLen = sizeof(msrcmd_t);
	IOByteCount dataOutLen = sizeof(msrcmd_t);
	msrcmd_t in, out;
	msr_t ret = { -1, -1};

	in.index = addr;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kReadMSR, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kReadMSR, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return ret;

	ret.lo = out.lo;
	ret.hi = out.hi;

	return ret;
}

int wrmsr(int addr, msr_t msr)
{
	kern_return_t err;
	IOByteCount dataInLen = sizeof(msrcmd_t);
	IOByteCount dataOutLen = sizeof(msrcmd_t);
	msrcmd_t in, out;

	in.index = addr;
	in.lo = msr.lo;
	in.hi = msr.hi;

#if !defined(__LP64__) && defined(WANT_OLD_API)
	/* Check if OSX 10.5 API is available */
	if (IOConnectCallStructMethod != NULL) {
#endif
		err = IOConnectCallStructMethod(connect, kWriteMSR, &in, dataInLen, &out, &dataOutLen);
#if !defined(__LP64__) && defined(WANT_OLD_API)
	} else {
		/* Use old API */
		err = IOConnectMethodStructureIStructureO(connect, kWriteMSR, dataInLen, &dataOutLen, &in, &out);
	}
#endif

	if (err != KERN_SUCCESS)
		return 1;

	return 0;
}


