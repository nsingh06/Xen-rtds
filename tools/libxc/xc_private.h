
#ifndef XC_PRIVATE_H
#define XC_PRIVATE_H

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "xenctrl.h"

#include <xen/sys/privcmd.h>

/* valgrind cannot see when a hypercall has filled in some values.  For this
   reason, we must zero the privcmd_hypercall_t or domctl/sysctl instance
   before a call, if using valgrind.  */
#ifdef VALGRIND
#define DECLARE_HYPERCALL privcmd_hypercall_t hypercall = { 0 }
#define DECLARE_DOMCTL struct xen_domctl domctl = { 0 }
#define DECLARE_SYSCTL struct xen_sysctl sysctl = { 0 }
#else
#define DECLARE_HYPERCALL privcmd_hypercall_t hypercall
#define DECLARE_DOMCTL struct xen_domctl domctl
#define DECLARE_SYSCTL struct xen_sysctl sysctl
#endif

#undef PAGE_SHIFT
#undef PAGE_SIZE
#undef PAGE_MASK
#define PAGE_SHIFT              XC_PAGE_SHIFT
#define PAGE_SIZE               (1UL << PAGE_SHIFT)
#define PAGE_MASK               (~(PAGE_SIZE-1))

#define DEBUG    1
#define INFO     1
#define PROGRESS 0

#if INFO
#define IPRINTF(_f, _a...) printf(_f , ## _a)
#else
#define IPRINTF(_f, _a...) ((void)0)
#endif

#if DEBUG
#define DPRINTF(_f, _a...) fprintf(stderr, _f , ## _a)
#else
#define DPRINTF(_f, _a...) ((void)0)
#endif

#if PROGRESS
#define PPRINTF(_f, _a...) fprintf(stderr, _f , ## _a)
#else
#define PPRINTF(_f, _a...)
#endif

#define ERROR(_m, _a...)                        \
do {                                            \
    int __saved_errno = errno;                  \
    DPRINTF("ERROR: " _m "\n" , ## _a );        \
    errno = __saved_errno;                      \
} while (0)

#define PERROR(_m, _a...)                               \
do {                                                    \
    int __saved_errno = errno;                          \
    DPRINTF("ERROR: " _m " (%d = %s)\n" , ## _a ,       \
            __saved_errno, strerror(__saved_errno));    \
    errno = __saved_errno;                              \
} while (0)

static inline void safe_munlock(const void *addr, size_t len)
{
    int saved_errno = errno;
    (void)munlock(addr, len);
    errno = saved_errno;
}

int do_xen_hypercall(int xc_handle, privcmd_hypercall_t *hypercall);

static inline int do_xen_version(int xc_handle, int cmd, void *dest)
{
    DECLARE_HYPERCALL;

    hypercall.op     = __HYPERVISOR_xen_version;
    hypercall.arg[0] = (unsigned long) cmd;
    hypercall.arg[1] = (unsigned long) dest;

    return do_xen_hypercall(xc_handle, &hypercall);
}

static inline int do_domctl(int xc_handle, struct xen_domctl *domctl)
{
    int ret = -1;
    DECLARE_HYPERCALL;

    domctl->interface_version = XEN_DOMCTL_INTERFACE_VERSION;

    hypercall.op     = __HYPERVISOR_domctl;
    hypercall.arg[0] = (unsigned long)domctl;

    if ( mlock(domctl, sizeof(*domctl)) != 0 )
    {
        PERROR("Could not lock memory for Xen hypercall");
        goto out1;
    }

    if ( (ret = do_xen_hypercall(xc_handle, &hypercall)) < 0 )
    {
        if ( errno == EACCES )
            DPRINTF("domctl operation failed -- need to"
                    " rebuild the user-space tool set?\n");
    }

    safe_munlock(domctl, sizeof(*domctl));

 out1:
    return ret;
}

static inline int do_sysctl(int xc_handle, struct xen_sysctl *sysctl)
{
    int ret = -1;
    DECLARE_HYPERCALL;

    sysctl->interface_version = XEN_SYSCTL_INTERFACE_VERSION;

    hypercall.op     = __HYPERVISOR_sysctl;
    hypercall.arg[0] = (unsigned long)sysctl;

    if ( mlock(sysctl, sizeof(*sysctl)) != 0 )
    {
        PERROR("Could not lock memory for Xen hypercall");
        goto out1;
    }

    if ( (ret = do_xen_hypercall(xc_handle, &hypercall)) < 0 )
    {
        if ( errno == EACCES )
            DPRINTF("sysctl operation failed -- need to"
                    " rebuild the user-space tool set?\n");
    }

    safe_munlock(sysctl, sizeof(*sysctl));

 out1:
    return ret;
}

int xc_map_foreign_ranges(int xc_handle, uint32_t dom,
                          privcmd_mmap_entry_t *entries, int nr);

#endif /* __XC_PRIVATE_H__ */
