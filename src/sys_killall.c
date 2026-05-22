/*
 * Copyright (C) 2026 - OS Assignment CO2018
 * System call killall - Terminate processes by program name
 * Syscall number: 101
 */

#include "common.h"
#include "syscall.h"
#include "queue.h"
#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Hàm đọc chuỗi tên chương trình từ vùng nhớ của process
 */
static int read_program_name(struct pcb_t *caller, arg_t region_id, 
                              char *buffer, int max_len)
{
    struct vm_rg_struct *region;
    addr_t addr;
    BYTE byte;
    int i = 0;
    
    /* Kiểm tra caller hợp lệ */
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL) {
        printf("killall: Invalid caller or kernel/mm structure\n");
        return -1;
    }
    
    /* Kiểm tra region_id hợp lệ */
    if (region_id < 0 || region_id >= PAGING_MAX_SYMTBL_SZ) {
        printf("killall: Invalid REGIONID %ld (max %d)\n", 
               (long)region_id, PAGING_MAX_SYMTBL_SZ);
        return -1;
    }
    
    /* Lấy vùng nhớ từ symbol table */
    region = &caller->krnl->mm->symrgtbl[region_id];
    
    /* Kiểm tra vùng nhớ có hợp lệ không */
    if (region->rg_start == 0 && region->rg_end == 0) {
        printf("killall: Memory region %ld is not allocated\n", (long)region_id);
        return -1;
    }
    
    /* Kiểm tra rg_start hợp lệ */
    if (region->rg_start == 0) {
        printf("killall: Invalid rg_start address\n");
        return -1;
    }
    
    /* Đọc từng byte từ vùng nhớ */
    addr = region->rg_start;
    
    printf("killall: Reading from address %ld\n", (long)addr);
    
    while (i < max_len - 1) {
        /* Sử dụng MEMPHY_read thay vì pg_getval để tránh lỗi phân trang */
        int phyaddr;
        int pgn = PAGING_PGN(addr);
        int off = PAGING_OFFST(addr);
        uint32_t pte;
        int fpn;
        
        /* Lấy PTE entry */
        pte = pte_get_entry(caller, pgn);
        
        if (pte == 0) {
            printf("killall: PTE is zero for page %d\n", pgn);
            break;
        }
        
        if (!PAGING_PAGE_PRESENT(pte)) {
            printf("killall: Page not present for address %ld\n", (long)addr);
            break;
        }
        
        fpn = PAGING_FPN(pte);
        phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
        
        if (caller->krnl->mram == NULL) {
            printf("killall: mram is NULL\n");
            return -1;
        }
        
        MEMPHY_read(caller->krnl->mram, phyaddr, &byte);
        
        buffer[i] = (char)byte;
        
        if (byte == '\0') {
            break;
        }
        
        addr++;
        i++;
    }
    
    buffer[i] = '\0';
    
    if (i == 0) {
        printf("killall: Empty program name\n");
        return -1;
    }
    
    printf("killall: Searching for program: '%s'\n", buffer);
    
    return 0;
}

/*
 * Hàm xóa tiến trình và giải phóng tài nguyên
 */
static int terminate_process(struct pcb_t *proc, struct krnl_t *krnl)
{
    if (proc == NULL)
        return -1;
    
    printf("killall: Terminating process PID: %d, Path: %s\n", 
           proc->pid, proc->path);
    
    /* Giải phóng code segment */
    if (proc->code != NULL) {
        if (proc->code->text != NULL)
            free(proc->code->text);
        free(proc->code);
    }
    
    /* Giải phóng page table */
    if (proc->page_table != NULL)
        free(proc->page_table);
    
    /* Giải phóng process control block */
    free(proc);
    
    return 0;
}

/*
 * Hàm tìm và xóa process khỏi MLQ ready queue
 */
static int remove_from_mlq_by_name(struct krnl_t *krnl, const char *target_name, 
                                    uint32_t caller_pid)
{
    int killed = 0;
    int prio, i;
    
    if (target_name == NULL || krnl == NULL)
        return 0;
    
#ifdef MLQ_SCHED
    for (prio = 0; prio < MAX_PRIO; prio++) {
        struct queue_t *queue = &krnl->mlq_ready_queue[prio];
        
        if (queue == NULL)
            continue;
            
        for (i = queue->size - 1; i >= 0; i--) {
            struct pcb_t *proc = queue->proc[i];
            
            if (proc == NULL)
                continue;
            
            if (proc->pid == caller_pid) {
                continue;
            }
            
            if (strstr(proc->path, target_name) != NULL) {
                /* Xóa khỏi queue */
                for (int j = i; j < queue->size - 1; j++) {
                    queue->proc[j] = queue->proc[j + 1];
                }
                queue->size--;
                
                terminate_process(proc, krnl);
                killed++;
            }
        }
    }
#endif
    
    return killed;
}

/*
 * Hàm xử lý system call killall
 */
int __sys_killall(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
    char prog_name[256];
    struct pcb_t *caller = NULL;
    int total_killed = 0;
    int i;
    
    printf("\n========================================\n");
    printf("SYSTEM CALL KILLALL (syscall 101)\n");
    printf("========================================\n");
    
    /* Kiểm tra tham số */
    if (krnl == NULL || regs == NULL) {
        printf("killall: Invalid kernel or registers\n");
        return -1;
    }
    
    /* Tìm process caller */
    if (krnl->running_list != NULL) {
        for (i = 0; i < krnl->running_list->size; i++) {
            struct pcb_t *proc = krnl->running_list->proc[i];
            if (proc != NULL && proc->pid == pid) {
                caller = proc;
                break;
            }
        }
    }
    
    if (caller == NULL) {
        printf("killall: Cannot find caller process with PID %d\n", pid);
        return -1;
    }
    
    printf("killall: Caller PID: %d, REGIONID: %ld\n", pid, (long)regs->a1);
    
    /* Đọc tên chương trình */
    if (read_program_name(caller, regs->a1, prog_name, sizeof(prog_name)) != 0) {
        printf("killall: Failed to read program name, using default\n");
        /* Thoát thay vì crash */
        return -1;
    }
    
    /* Kill các process */
    total_killed += remove_from_mlq_by_name(krnl, prog_name, pid);
    
    printf("========================================\n");
    printf("killall: %d process(es) terminated\n", total_killed);
    printf("========================================\n\n");
    
    return 0;
}