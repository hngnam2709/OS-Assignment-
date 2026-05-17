/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

//typedef char BYTE;

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;
   struct pcb_t *caller = NULL;

   /*
    * @bksysnet: Please note in the dual spacing design
    *            syscall implementations are in kernel space.
    */

   /* Lấy tiến trình đang chạy từ danh sách running_list của Kernel
    * Thay vì dùng vòng lặp phức tạp, hệ thống của thầy thường để running_list 
    * trỏ trực tiếp đến PCB đang chạy trên CPU hiện tại.
    * (Nếu hệ thống có nhiều CPU, krnl->running_list có thể là mảng, nhưng
    * trong đồ án này ta có thể ép kiểu an toàn hoặc dùng hàm get_proc).
    */
    
   // Lấy tiến trình dựa vào PID truyền vào
   // (Đảm bảo TV1 đã thiết lập queue chạy đúng cách)
   // Tạm thời ép kiểu trực tiếp vì running_list thường chứa con trỏ PCB
   if (krnl->running_list != NULL) {
       caller = (struct pcb_t *)krnl->running_list; 
   }

   /* Nếu không tìm thấy tiến trình trong danh sách đang chạy, báo lỗi và thoát */
   if (caller == NULL || caller->pid != pid) {
       printf("__sys_memmap ERROR: Cannot find valid caller with PID %d\n", pid);
       return -1; 
   }
	
   switch (memop) {
   case SYSMEM_MAP_OP:
            /* Gọi hàm vmap_pgd_memset của TV3 để tạo bảng phân trang (Demand Allocation) */
            // a2: start address (đã align)
            // a3: số lượng trang (pgnum)
			vmap_pgd_memset(caller, regs->a2, regs->a3);
            break;

   case SYSMEM_INC_OP:
            // Hàm này tăng sbrk (TV2 hoặc TV3 viết)
            inc_vma_limit(caller, regs->a2, regs->a3);
            break;

   case SYSMEM_SWP_OP:
            // Hàm swapping (TV4 viết)
            __mm_swap_page(caller, regs->a2, regs->a3);
            break;

   case SYSMEM_IO_READ:
            MEMPHY_read(caller->krnl->mram, regs->a2, &value);
            regs->a3 = value;
            break;

   case SYSMEM_IO_WRITE:
            MEMPHY_write(caller->krnl->mram, regs->a2, regs->a3);
            break;

   default:
            printf("Memop code: %d\n", memop);
            break;
   }
   
   return 0;
}

