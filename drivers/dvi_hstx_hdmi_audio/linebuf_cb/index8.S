.syntax unified
.cpu cortex-m33
.section .scratch_y.linebuf_cb.index8, "ax"

#include "cb_defs.h"

.global linebuf_cb_index8_a
.global linebuf_cb_index8_a_end
.type linebuf_cb_index8_a,%function
.thumb_func
.align 2

// R0 - task ptr, R1 - priv ptr
linebuf_cb_index8_a:
    push        {r4-r9, lr}

    ldrh        r3, [r0, cb_task_line]
    ldrh        r4, [r0, cb_task_width]
    ldrh        r5, [r0, cb_task_height]
    ldrh        r6, [r0, cb_task_pitch_fixup]
    ldr         r0, [r0, cb_task_dst]               // r0 - dst
    ldr         r7, [r1, 4]                         // r7 - palette
    ldr         r2, [r1, 0]                         // r1 - src
    mla         r2, r3, r4, r2                      // src += task->line * task->width

.Ly_loop:
    mov         r3, r4
.Lx_loop:
.rept 8
    ldrb        r8, [r2], 1
    ldrb        r9, [r2], 1
    ldr         r8, [r7, r8, lsl 2]
    ldr         r9, [r7, r9, lsl 2]
    str         r8, [r0], 4
    str         r9, [r0], 4
.endr
    subs        r3, 16
    bne         .Lx_loop
    
    add         r0, r6
    subs        r5, 1
    bne         .Ly_loop

    pop         {r4-r9, pc}

linebuf_cb_index8_a_end:
