#include <am.h>
#include <x86.h>
#include <klib.h>

static _Context* (*user_handler)(_Event, _Context*) = NULL;

void vectrap();
void vecsys();
void vecnull();
void irq0();

void get_cur_as(_Context *c);
void _switch(_Context *c);

// struct _Context {
//   uintptr_t esp;
//   struct _Protect *prot;
//   uintptr_t edi, esi, ebp, ori_esp, ebx, edx, ecx, eax;
//   int       irq;
//   uintptr_t err, eip, cs, eflags;

// };

_Context* irq_handle(_Context *tf) {
  _Context *next = tf;
  get_cur_as(tf);
  // printf("irq_handle:\n");
  // printf("eax: 0x%x, ecx: 0x%x, edx: 0x%x, ebx: 0x%x, esp: 0x%x, ebp: 0x%x, esi: 0x%x, edi: 0x%x, eflags: 0x%x\n", 
  // tf->eax, tf->ecx, tf->edx, tf->ebx, tf->esp, tf->ebp, tf->esi, tf->edi, tf->eflags);
  // printf("esp: 0x%x, prot: 0x%x, edi: 0x%x, esi: 0x%x, ebp: 0x%x, ori_esp: 0x%x, ebx: 0x%x, edx: 0x%x, ecx: 0x%x, eax: 0x%x, irq: 0x%x, err: 0x%x, eip: 0x%x, cs: 0x%x, eflags: 0x%x\n", 
  // tf->esp, tf->prot, tf->edi, tf->esi, tf->ebp, tf->ori_esp, tf->ebx, tf->edx, tf->ecx, tf->eax, tf->irq, tf->err, tf->eip, tf->cs, tf->eflags);;
  // printf("prot: 0x%x, edi: 0x%x, esi: 0x%x, ebp: 0x%x, ori_esp: 0x%x, ebx: 0x%x, edx: 0x%x, ecx: 0x%x, eax: 0x%x, irq: 0x%x, err: 0x%x, eip: 0x%x, cs: 0x%x, eflags: 0x%x\n", 
  // tf->prot, tf->edi, tf->esi, tf->ebp, tf->ori_esp, tf->ebx, tf->edx, tf->ecx, tf->eax, tf->irq, tf->err, tf->eip, tf->cs, tf->eflags);;

  if (user_handler) {
    _Event ev = {0};
    // printf("irq: %x\n", tf->irq);
    switch (tf->irq) {
      case 0x20: ev.event = _EVENT_IRQ_TIMER; break;
      case 0x80: ev.event = _EVENT_SYSCALL; break;
      case 0x81: ev.event = _EVENT_YIELD; break;
      default: ev.event = _EVENT_ERROR; break;
    }

    next = user_handler(ev, tf);
    if (next == NULL) {
      next = tf;
    }
  }

  _switch(next);
  return next;
}

static GateDesc idt[NR_IRQ];

int _cte_init(_Context*(*handler)(_Event, _Context*)) {
  // initialize IDT
  for (unsigned int i = 0; i < NR_IRQ; i ++) {
    idt[i] = GATE(STS_TG32, KSEL(SEG_KCODE), vecnull, DPL_KERN);
  }

  // -------------------- system call --------------------------
  idt[32] = GATE(STS_TG32, KSEL(SEG_KCODE), irq0, DPL_KERN);
  idt[0x80] = GATE(STS_TG32, KSEL(SEG_KCODE), vecsys, DPL_KERN);
  idt[0x81] = GATE(STS_TG32, KSEL(SEG_KCODE), vectrap, DPL_KERN);

  set_idt(idt, sizeof(idt));

  // register event handler
  user_handler = handler;

  return 0;
}

_Context *_kcontext(_Area stack, void (*entry)(void *), void *arg) {
  printf("_kcontext: stack.start: 0x%x stack.end: 0x%x\n", stack.start, stack.end);
  _Context* p = (_Context *)(stack.end - sizeof(_Context));
  printf("_kcontext: p: 0x%x size: %d\n", p, sizeof(_Context));
  p->cs = 8;
  p->eflags = 2;
  p->eip = (uint32_t)entry;

  *(uintptr_t *)stack.start = (uintptr_t)p; 
  return p;
}

void _yield() {
  asm volatile("int $0x81");
}

int _intr_read() {
  return 0;
}

void _intr_write(int enable) {
}
