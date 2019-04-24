# arm64-kvm-helloworld
## How to run

just run "run.sh" in the top folder with "sudo",

	sudo ./run.sh

## How it works

This is the simplest kvm helloworld code for arm64.

The guest folder contains code for guest. The idea is just simple to write
character 'h', 'e', 'l', 'l', 'o' to mmio address 0x1100, like following

__head:
	mov x3, 0x1100

	mov w4, 'h'
	strb w4, [x3]
	...

The guest code will be,
1) comipled to a.out.
2) The binary code is extract from a.out via objdump.
3) the binary code will be convert to an char array in
	../host/guest_code.h

All steps are in the mk file in guest folder, I don't use makefile as
using command in mk file is simpler and more straightforward.

The host folder contains host code to do some necessary kvm check and
create vm, cpu, memory, etc.

The vm's memory start and size is defined as,
	#define MEM_SIZE 0x10000
	#define GUEST_PHY_START 0x10000

	struct kvm_userspace_memory_region region = {
		.slot = 0,
		.guest_phys_addr = GUEST_PHY_START,
		.memory_size = MEM_SIZE,
	};


The guest code is copied to vm's memroy via memcpy, the memory region
is set to guest via ioctl KVM_SET_USER_MEMORY_REGION,

	memcpy(mem, guest_code, guest_code_len);
	region.userspace_addr = (__u64)mem;
	ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);

So, as the guest will write to address 0x1100, it is outside the vm's
memory address range[0x10000, 0x20000]. This will cause a KVM_EXIT_MMIO
in run->exit_reason. We output related infomation about exit address and
the data guest has just written, that is 'h', 'e','l', 'l', 'o',

	case KVM_EXIT_MMIO:
		printf("unhandled KVM_EXIT_MMIO address:%x write:%d data:%x len:%d\n",
		       run->mmio.phys_addr,  run->mmio.is_write, run->mmio.data,
		       run->mmio.len);
		{
			unsigned char *pdata = (unsigned char *)run->mmio.data;
			int i;
			for (i = 0; i < run->mmio.len; i++) {
				printf("%x %c\n", pdata[i], pdata[i]);
			}
		}

		break;

The code has been tested on QCOM QDF2400 server. As it is generic code, I
think it can run on other ARMv8 platform too.
