// switch_arm64.s - Bare-metal ARM64 context switcher
// Calling Convention:
//   x0 = void **old_sp  (Pointer to old coroutine's SP storage)
//   x1 = void *new_sp   (New coroutine's stack pointer to load)

.global _co_switch
.align 4

_co_switch:
    // 1. Save all callee-saved registers onto the current stack
    // Registers x19-x28, x29 (FP), and x30 (LR) must be preserved across calls
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!

    // 2. Save current SP into *old_sp (pointed to by x0)
    mov x2, sp
    str x2, [x0]

    // 3. Switch CPU Stack Pointer to new_sp (passed in x1)!
    mov sp, x1

    // 4. Restore all callee-saved registers from the NEW stack
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16

    // 5. Jump to restored x30 (Link Register - the next instruction!)
    ret
