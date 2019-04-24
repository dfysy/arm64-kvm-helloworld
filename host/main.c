#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <sys/mman.h>
#include <asm-arm64/kvm.h>
#include "errno-base.h"
#include "guest_code.h"
#include <asm/ptrace.h>

#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define MEM_SIZE 0x10000
#define GUEST_PHY_START 0x10000
#define QEMU_KVM_ARM_TARGET_AEM_V8 0
#define ARM64_CORE_REG(x)	(KVM_REG_ARM64 | KVM_REG_SIZE_U64 | \
				 KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(x))

void die_perror(const char *s)
{
	perror(s);
	exit(1);
}

int main(int argc, char *argv[])
{
	int ret;
	void *mem;
	int kvm, vmfd, vcpufd;
	int vcpu_mmap_size;
	struct kvm_run *run;
	struct kvm_one_reg reg;
	__u64 data;
	struct kvm_vcpu_init init;
	struct kvm_vcpu_init preferred_init;
	struct kvm_coalesced_mmio_zone zone;
	struct kvm_userspace_memory_region region = {
		.slot = 0,
		.guest_phys_addr = GUEST_PHY_START,
		.memory_size = MEM_SIZE,
	};

	kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
	if (kvm < 0)
	printf("open kvm device node failed\n");	

	ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
	printf("KVM version is:%d\n", ret);
	
	if (ret != 12) {
		printf("kvm version not supported\n");
		return -ERANGE;
	}

	ret = ioctl(kvm, KVM_CHECK_EXTENSION, KVM_CAP_USER_MEMORY);
	if (!ret) {
		printf("kvm user memory capability unavailable\n");
		return -EINVAL;
	}

	vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long)0);
	if (vmfd < 0) {
		printf("create kvm failed\n");
		return -ENODEV;
	}
	
	mem = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1 , 0);
	if (mem == MAP_FAILED) {
		printf("mmap memory failed\n");
		return -ENOMEM;
	}
	
	memcpy(mem, guest_code, guest_code_len);
	region.userspace_addr = (__u64)mem;	
	ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
	if (ret) {
		printf("set user space memory failed\n");
		return -EFAULT;
	}

	vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
	if (vcpufd < 0) {
		printf("create vcpu failed\n");
		return -ENODEV;
	}
		
	vcpu_mmap_size = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
	run = (struct kvm_run *)mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);

	memset(init.features, 0, sizeof(init.features));
	//init.features[0] |= 1 << KVM_ARM_VCPU_POWER_OFF;

	ret = ioctl(kvm, KVM_CHECK_EXTENSION, KVM_CAP_ARM_PSCI_0_2);
	if (ret > 0) {
		printf("init PSCI\n");
		init.features[0] |= 1 << KVM_ARM_VCPU_PSCI_0_2;
	}

	ret = ioctl(kvm, KVM_CHECK_EXTENSION, KVM_CAP_ARM_PMU_V3);
	if(ret > 0) {
		printf("init PMU V3\n");
		init.features[0] |= 1 << KVM_ARM_VCPU_PMU_V3;
	}

	ret = ioctl(vmfd, KVM_ARM_PREFERRED_TARGET, &preferred_init);
	if (!ret) {
		printf("perferred init got, target:%d feature[0]:%x\n",
		       preferred_init.target, preferred_init.features[0]);
		memcpy(&init, &preferred_init, sizeof(init));
	} else {
		printf("no perferred init\n");
		init.target = QEMU_KVM_ARM_TARGET_AEM_V8;
	}

	ret = ioctl(vcpufd, KVM_ARM_VCPU_INIT, &init);
	if (ret) {
		printf("arm vcpu init failed:%d\n", ret);
		return -EINVAL;
	}

	/*reset cpu*/
	reg.addr = (__u64)&data;

	/* pstate = all interrupts masked */
	data	= PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT | PSR_MODE_EL1h;
	reg.id	= ARM64_CORE_REG(regs.pstate);
	if (ioctl(vcpufd, KVM_SET_ONE_REG, &reg) < 0)
		die_perror("KVM_SET_ONE_REG failed (spsr[EL1])");

	/* x1...x3 = 0 */
	data	= 0;
	reg.id	= ARM64_CORE_REG(regs.regs[1]);
	if (ioctl(vcpufd, KVM_SET_ONE_REG, &reg) < 0)
		die_perror("KVM_SET_ONE_REG failed (x1)");

	reg.id	= ARM64_CORE_REG(regs.regs[2]);
	if (ioctl(vcpufd, KVM_SET_ONE_REG, &reg) < 0)
		die_perror("KVM_SET_ONE_REG failed (x2)");

	reg.id	= ARM64_CORE_REG(regs.regs[3]);
	if (ioctl(vcpufd, KVM_SET_ONE_REG, &reg) < 0)
		die_perror("KVM_SET_ONE_REG failed (x3)");

	data	= GUEST_PHY_START;
	reg.id	= ARM64_CORE_REG(regs.pc);
	if (ioctl(vcpufd, KVM_SET_ONE_REG, &reg) < 0)
		die_perror("KVM_SET_ONE_REG failed (pc)");
#if 0
	printf("arm vcpu init done:%d\n", ret);

	{
		int i;
		unsigned char *data = (unsigned char *) mem;
		for (i = 0; i < 64; i++) {
			printf("0x%2x ", data[i]);
		}
	}
#endif
#if 0
	zone.addr = 0x00;
	zone.size = 0x10000;
	ret = ioctl(vmfd, KVM_REGISTER_COALESCED_MMIO, &zone);
	if (ret < 0)
		die_perror("MMIO register failed");
#endif
	while (1) {
		printf("before run\n");
		ioctl(vcpufd, KVM_RUN, NULL);
		printf("after run\n");
		switch (run->exit_reason) {
			case KVM_EXIT_IO:
				if (run->io.direction == KVM_EXIT_IO_OUT &&
				    run->io.size == 1 &&
				    run->io.port == 0x87654321 &&
				    run->io.count == 1)
					putchar(*(((char *)run) + run->io.data_offset));
				else
				    printf("unhandled KVM_EXIT_IO\n");
			break;
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
			default:
				printf("unhandled reason:%d\n", run->exit_reason);
		}
	}
	return 0;
}


