/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}


/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Extract page direactories */
	  *pgd = PAGING64_ADDR_PGD(addr);
    *p4d = PAGING64_ADDR_P4D(addr);
    *pud = PAGING64_ADDR_PUD(addr);
    *pmd = PAGING64_ADDR_PMD(addr);
    *pt  = PAGING64_ADDR_PT(addr);

	/* TODO: implement the page direactories mapping */

	return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}


/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
    struct krnl_t *krnl = caller->krnl;
    addr_t pgd_idx = 0, p4d_idx = 0, pud_idx = 0, pmd_idx = 0, pt_idx = 0;
    
    addr_t vaddr = pgn << PAGING64_ADDR_PT_SHIFT;
    get_pd_from_address(vaddr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    if (krnl->mm->pgd == NULL) return -1;
    
    addr_t* p4d_array = (addr_t*) krnl->mm->pgd[pgd_idx];
    if (p4d_array == NULL) return -1;
    
    addr_t* pud_array = (addr_t*) p4d_array[p4d_idx];
    if (pud_array == NULL) return -1;
    
    addr_t* pmd_array = (addr_t*) pud_array[pud_idx];
    if (pmd_array == NULL) return -1;
    
    addr_t* pt_array = (addr_t*) pmd_array[pmd_idx];
    if (pt_array == NULL) return -1;

    addr_t* pte = &pt_array[pt_idx];
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

    return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
    struct krnl_t *krnl = caller->krnl;
    addr_t pgd_idx = 0, p4d_idx = 0, pud_idx = 0, pmd_idx = 0, pt_idx = 0;
    
    // Tính lại địa chỉ ảo từ page number
    addr_t vaddr = pgn << PAGING64_ADDR_PT_SHIFT;
    get_pd_from_address(vaddr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    // Duyệt cây (Demand Allocation kiểm tra an toàn)
    if (krnl->mm->pgd == NULL) return -1;
    
    addr_t* p4d_array = (addr_t*) krnl->mm->pgd[pgd_idx];
    if (p4d_array == NULL) return -1;
    
    addr_t* pud_array = (addr_t*) p4d_array[p4d_idx];
    if (pud_array == NULL) return -1;
    
    addr_t* pmd_array = (addr_t*) pud_array[pud_idx];
    if (pmd_array == NULL) return -1;
    
    addr_t* pt_array = (addr_t*) pmd_array[pmd_idx];
    if (pt_array == NULL) return -1;

    // Cài đặt FPN cho PTE
    addr_t* pte = &pt_array[pt_idx];
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

    return 0;
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
    struct krnl_t *krnl = caller->krnl;
    addr_t pgd_idx = 0, p4d_idx = 0, pud_idx = 0, pmd_idx = 0, pt_idx = 0;
    
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    if (krnl->mm->pgd == NULL) return 0;
    
    addr_t* p4d_array = (addr_t*) krnl->mm->pgd[pgd_idx];
    if (p4d_array == NULL) return 0;
    
    addr_t* pud_array = (addr_t*) p4d_array[p4d_idx];
    if (pud_array == NULL) return 0;
    
    addr_t* pmd_array = (addr_t*) pud_array[pud_idx];
    if (pmd_array == NULL) return 0;
    
    addr_t* pt_array = (addr_t*) pmd_array[pmd_idx];
    if (pt_array == NULL) return 0;

    return (uint32_t) pt_array[pt_idx];
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	  struct krnl_t *krnl = caller->krnl;
	  uint32_t pte = 0;
    
    addr_t pgd_idx = 0;
    addr_t p4d_idx = 0;
    addr_t pud_idx = 0;
    addr_t pmd_idx = 0;
    addr_t pt_idx = 0;

    /* Lấy các tọa độ (index) 5 cấp từ page number */
    get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

    /* Bắt đầu duyệt cây Page Table từ PGD xuống */
    // Nếu chưa khởi tạo PGD, chắc chắn chưa có mapping
    if (krnl->mm->pgd == NULL) return 0;
    
    addr_t* p4d_array = (addr_t*) krnl->mm->pgd[pgd_idx];
    if (p4d_array == NULL) return 0; // Giá trị 0 tương đương con trỏ NULL
    
    addr_t* pud_array = (addr_t*) p4d_array[p4d_idx];
    if (pud_array == NULL) return 0;
    
    addr_t* pmd_array = (addr_t*) pud_array[pud_idx];
    if (pmd_array == NULL) return 0;
    
    addr_t* pt_array = (addr_t*) pmd_array[pmd_idx];
    if (pt_array == NULL) return 0;

    /* Trích xuất giá trị PTE tại cấp cuối cùng (Page Table) */
    // Ép kiểu về uint32_t theo đúng signature của hàm
    pte = (uint32_t) pt_array[pt_idx];

    return pte;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  //int pgit = 0;
  //uint64_t pattern = 0xdeadbeef;

  /* TODO memset the page table with given pattern
   */
    struct mm_struct *mm = caller->krnl->mm;
    int pgit = 0;

    for (pgit = 0; pgit < pgnum; pgit++) {
        addr_t current_addr = addr + pgit * PAGING64_PAGESZ; // Thay PAGE_SIZE bằng PAGING64_PAGESZ

        // Sử dụng Macro chính hãng từ mm64.h
        addr_t pgd_idx = PAGING64_ADDR_PGD(current_addr);
        addr_t p4d_idx = PAGING64_ADDR_P4D(current_addr);
        addr_t pud_idx = PAGING64_ADDR_PUD(current_addr);
        addr_t pmd_idx = PAGING64_ADDR_PMD(current_addr);
        addr_t pt_idx  = PAGING64_ADDR_PT(current_addr);

        // 1. Cấp phát PGD nếu chưa có (NUM_ENTRIES_PER_LEVEL = 512, vì 2^9 = 512)
        if (mm->pgd == NULL) {
            mm->pgd = (addr_t*) calloc(512, sizeof(addr_t));
        }
        
        if (mm->pgd[pgd_idx] == 0) {
            addr_t* new_p4d = (addr_t*) calloc(512, sizeof(addr_t));
            mm->pgd[pgd_idx] = (addr_t) new_p4d; 
        }

        addr_t* p4d_array = (addr_t*) mm->pgd[pgd_idx];
        if (p4d_array[p4d_idx] == 0) {
             addr_t* new_pud = (addr_t*) calloc(512, sizeof(addr_t));
             p4d_array[p4d_idx] = (addr_t) new_pud;
        }

        addr_t* pud_array = (addr_t*) p4d_array[p4d_idx];
        if (pud_array[pud_idx] == 0) {
             addr_t* new_pmd = (addr_t*) calloc(512, sizeof(addr_t));
             pud_array[pud_idx] = (addr_t) new_pmd;
        }

        addr_t* pmd_array = (addr_t*) pud_array[pud_idx];
        if (pmd_array[pmd_idx] == 0) {
             addr_t* new_pt = (addr_t*) calloc(512, sizeof(addr_t));
             pmd_array[pmd_idx] = (addr_t) new_pt;
        }

        addr_t* pt_array = (addr_t*) pmd_array[pmd_idx];

        // Khởi tạo PTE rỗng (Present = 0)
        init_pte(&pt_array[pt_idx], 0, 0, 0, 0, 0, 0); 
    }

    return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   
    struct framephy_struct *fpit = frames;
    int pgit = 0;
    addr_t pgn;

    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + pgnum * PAGING64_PAGESZ; 
    ret_rg->vmaid = 0; 

    for (pgit = 0; pgit < pgnum; pgit++) {
        addr_t current_vaddr = addr + pgit * PAGING64_PAGESZ;
        pgn = current_vaddr >> PAGING64_ADDR_PT_SHIFT; 

        if (fpit != NULL) {
            pte_set_fpn(caller, pgn, fpit->fpn);
            enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
            fpit = fpit->fp_next;
        } else {
            return -1;
        }
    }
    return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    addr_t fpn;
    int pgit;
    struct framephy_struct *newfp_str = NULL;
    struct framephy_struct *current_fp = NULL;

    *frm_lst = NULL;

    for (pgit = 0; pgit < req_pgnum; pgit++) {
        if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0) {
            newfp_str = malloc(sizeof(struct framephy_struct));
            newfp_str->fpn = fpn;
            newfp_str->owner = caller->krnl->mm; 
            newfp_str->fp_next = NULL;

            if (*frm_lst == NULL) {
                *frm_lst = newfp_str;
                current_fp = newfp_str;
            } else {
                current_fp->fp_next = newfp_str;
                current_fp = newfp_str;
            }
        } else { 
            return -3000; // Out of Memory
        }
    }
    return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
//int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  // ret_alloc = alloc_pages_range(caller, pgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
   vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
    struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

    /* Khởi tạo các rễ phân trang về NULL */
    mm->pgd = NULL;
    mm->p4d = NULL;
    mm->pud = NULL;
    mm->pmd = NULL;
    mm->pt = NULL;

    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = vma0->vm_start;
    vma0->sbrk = vma0->vm_start;
    struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
    enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

    vma0->vm_next = NULL;
    vma0->vm_mm = mm; 
    mm->mmap = vma0;
    for (int i = 0; i < MAX_CACHE_POOLS; i++) { mm->kcpooltbl[i] = NULL; }
    mm->fifo_pgn = NULL;

    return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
    struct krnl_t *krnl = caller->krnl;
    
    if (krnl == NULL || krnl->mm == NULL || krnl->mm->pgd == NULL) {
        printf("print_pgtbl: PGD is NULL. No memory mapped.\n");
        return -1;
    }

    printf("========== PAGE TABLE DUMP ==========\n");
    printf("Range: " FORMATX_ADDR " -> " FORMATX_ADDR "\n", (void *)start, (void *)end);
    
    addr_t start_pgn = start >> PAGING64_ADDR_PT_SHIFT; 
    addr_t end_pgn = end >> PAGING64_ADDR_PT_SHIFT;
    
    addr_t pgn;
    int entry_count = 0;

    for (pgn = start_pgn; pgn <= end_pgn; pgn++) {
        uint32_t pte = pte_get_entry(caller, pgn);
        
        if (pte != 0) {
            addr_t vaddr = pgn << PAGING64_ADDR_PT_SHIFT;
            
            int is_present = (pte >> 31) & 1; 
            int is_swapped = (pte >> 30) & 1; 
            addr_t fpn = pte & 0x1FFF;        

            printf("  [vaddr: " FORMATX_ADDR " | pgn: %4llu] -> PTE: %08x ", (void *)vaddr, (unsigned long long)pgn, pte);
            
            if (is_present && !is_swapped) {
                printf("(ONLINE  - FPN: %llu)\n", (unsigned long long)fpn);
            } else if (is_swapped) {
                printf("(SWAPPED - To Disk)\n");
            } else {
                printf("(UNKNOWN STATE)\n");
            }
            entry_count++;
        }
    }

    if (entry_count == 0) {
        printf("  -> No valid entries found in this range.\n");
    }
    printf("=====================================\n\n");

    return 0;
}

#endif  //def MM64
