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
 * @param caller: process gọi syscall
 * @param region_id: REGIONID chứa địa chỉ chuỗi tên
 * @param buffer: nơi lưu chuỗi đọc được
 * @param max_len: độ dài tối đa của buffer
 * @return 0 nếu thành công, -1 nếu thất bại
 */
static int read_program_name(struct pcb_t *caller, arg_t region_id, 
                              char *buffer, int max_len)
{
    struct vm_rg_struct *region;
    addr_t addr;
    BYTE byte;
    int i = 0;
    
    /* Lấy vùng nhớ từ symbol table theo region_id */
    if (region_id < 0 || region_id >= PAGING_MAX_SYMTBL_SZ) {
        printf("killall: Invalid REGIONID %ld\n", (long)region_id);
        return -1;
    }
    
    region = &caller->krnl->mm->symrgtbl[region_id];
    
    /* Kiểm tra vùng nhớ có hợp lệ không */
    if (region->rg_start == 0 && region->rg_end == 0) {
        printf("killall: Memory region %ld is not allocated\n", (long)region_id);
        return -1;
    }
    
    /* Đọc từng byte từ vùng nhớ cho đến khi gặp ký tự kết thúc '\0' */
    addr = region->rg_start;
    
    while (i < max_len - 1) {
        /* Đọc 1 byte từ bộ nhớ */
        if (pg_getval(caller->krnl->mm, addr, &byte, caller) != 0) {
            printf("killall: Failed to read memory at address %ld\n", (long)addr);
            return -1;
        }
        
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
 * @param proc: process cần kết thúc
 * @param krnl: kernel structure
 * @return 0 nếu thành công
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
    
    /* Giải phóng memory management struct */
#ifdef MM_PAGING
    if (proc->krnl != NULL && proc->krnl->mm != NULL) {
        /* TODO: Giải phóng các vùng nhớ trong mm_struct */
        /* Giải phóng vm_area_struct */
        struct vm_area_struct *vma = proc->krnl->mm->mmap;
        while (vma != NULL) {
            struct vm_area_struct *next = vma->vm_next;
            
            /* Giải phóng danh sách free region */
            struct vm_rg_struct *rg = vma->vm_freerg_list;
            while (rg != NULL) {
                struct vm_rg_struct *next_rg = rg->rg_next;
                free(rg);
                rg = next_rg;
            }
            
            free(vma);
            vma = next;
        }
        
        free(proc->krnl->mm);
        proc->krnl->mm = NULL;
    }
#endif
    
    /* Giải phóng process control block */
    free(proc);
    
    return 0;
}

/*
 * Hàm tìm và xóa process khỏi MLQ ready queue dựa trên path
 * @param krnl: kernel structure
 * @param target_name: tên chương trình cần tìm
 * @param caller_pid: PID của process gọi (không kill chính mình)
 * @return số lượng process đã kill
 */
static int remove_from_mlq_by_name(struct krnl_t *krnl, const char *target_name, 
                                    uint32_t caller_pid)
{
    int killed = 0;
    int prio, i;
    
    if (target_name == NULL)
        return 0;
    
#ifdef MLQ_SCHED
    for (prio = 0; prio < MAX_PRIO; prio++) {
        struct queue_t *queue = &krnl->mlq_ready_queue[prio];
        
        /* Duyệt ngược từ cuối để dễ xóa */
        for (i = queue->size - 1; i >= 0; i--) {
            struct pcb_t *proc = queue->proc[i];
            
            if (proc == NULL)
                continue;
            
            /* Không kill process đang gọi syscall */
            if (proc->pid == caller_pid) {
                printf("killall: Skipping caller process PID: %d\n", caller_pid);
                continue;
            }
            
            /* So sánh tên chương trình */
            if (strcmp(proc->path, target_name) == 0 ||
                strstr(proc->path, target_name) != NULL) {
                
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
 * Hàm tìm và xóa process khỏi running_list dựa trên path
 * @param krnl: kernel structure
 * @param target_name: tên chương trình cần tìm
 * @param caller_pid: PID của process gọi
 * @return số lượng process đã kill
 */
static int remove_from_running_by_name(struct krnl_t *krnl, const char *target_name,
                                        uint32_t caller_pid)
{
    int killed = 0;
    int i;
    
    if (krnl->running_list == NULL || target_name == NULL)
        return 0;
    
    for (i = 0; i < krnl->running_list->size; i++) {
        struct pcb_t *proc = krnl->running_list->proc[i];
        
        if (proc == NULL)
            continue;
        
        if (proc->pid == caller_pid) {
            continue;
        }
        
        if (strcmp(proc->path, target_name) == 0 ||
            strstr(proc->path, target_name) != NULL) {
            
            krnl->running_list->proc[i] = NULL;
            terminate_process(proc, krnl);
            killed++;
        }
    }
    
    return killed;
}

/*
 * Hàm xử lý system call killall
 * @param krnl: kernel structure
 * @param pid: PID của process gọi
 * @param regs: thanh ghi chứa tham số (a1 = REGIONID)
 * @return 0 nếu thành công, -1 nếu thất bại
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
    
    /* Tìm process caller dựa trên PID */
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
    
    /* Đọc tên chương trình từ vùng nhớ của caller */
    if (read_program_name(caller, regs->a1, prog_name, sizeof(prog_name)) != 0) {
        printf("killall: Failed to read program name from REGIONID %ld\n", 
               (long)regs->a1);
        return -1;
    }
    
    printf("killall: Target program name: '%s'\n", prog_name);
    
    /* Kill các process trong ready queue */
    total_killed += remove_from_mlq_by_name(krnl, prog_name, pid);
    
    /* Kill các process đang chạy (running list) */
    total_killed += remove_from_running_by_name(krnl, prog_name, pid);
    
    /* In kết quả */
    printf("========================================\n");
    printf("killall: %d process(es) terminated\n", total_killed);
    printf("========================================\n\n");
    
    return 0;
}