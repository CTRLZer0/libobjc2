#define ARGUMENT_SPILL_SIZE (8*10 + 8*16)

/* Windows ARM64 Exception Handling
 *
 * Structured Exception Handling (SEH) on Windows ARM64 differs from the x64
 * implementation. Functions consist of a single prologue and zero or more
 * epilogues. Instead of using offsets for the .seh* directives to manipulate the
 * stack frame, each directive corresponds to a single instruction.
 *
 * This presents a challenge for our objc_msgSend function, which only modifies
 * the stack when a slow lookup is needed (see label "5").
 *
 * To address this, we move the directive marking the start of a function deep
 * into the msgSend body to prevent marking every instruction as ".seh_nop."
 *
 *   For Windows:
 *  - EH_START(x): Start of function (no effect on Windows)
 *  - EH_END(x): End of function (no effect on Windows)
 *  - EH_START_AT_OFFSET(x): Mark Start of function (Delayed)
 *  - EH_END_AT_OFFSET(x): Mark End of function (Delayed)
 *  - EH_END_PROLOGUE: End of function prologue
 *  - EH_START_EPILOGUE: Start of function epilogue
 *  - EH_END_EPILOGUE: End of function epilogue
 *  - EH_SAVE_FP_LR(x): Save Frame Pointer and Link Register
 *  - EH_STACK_ALLOC(x): Stack allocation (inside prologue)
 *  - EH_ADD_FP(x): Add to Frame Pointer
 *  - EH_NOP: Mark instruction with no unwinding relevance
 *
 * For non-64-bit Windows systems or other platforms, these macros have no effect and can be used without causing issues.
 */

#ifdef _WIN32
#   define EH_START
#   define EH_END

#   define EH_START_AT_OFFSET(x) .seh_proc x
#   define EH_END_AT_OFFSET(x) .seh_endproc x

#   define EH_END_PROLOGUE .seh_endprologue
#   define EH_START_EPILOGUE .seh_startepilogue
#   define EH_END_EPILOGUE .seh_endepilogue

#   define EH_SAVE_FP_LR(x) .seh_save_fplr x
#   define EH_STACK_ALLOC(x) .seh_stackalloc x
#   define EH_ADD_FP(x) .seh_add_fp	x

#   define EH_NOP .seh_nop
#else
// Marks the real start and end of the function
#   define EH_START .cfi_startproc
#   define EH_END .cfi_endproc

// The following directives are either not
// needed or not available with CFI
#   define EH_START_AT_OFFSET(x)
#   define EH_END_AT_OFFSET(x)
#   define EH_END_PROLOGUE
#   define EH_START_EPILOGUE
#   define EH_END_EPILOGUE
#   define EH_SAVE_FP_LR(x)
#   define EH_STACK_ALLOC(x)
#   define EH_ADD_FP(x)
#   define EH_NOP
#endif

#ifdef OBJC2_TRACING_SUPPORTED
#define TRACE_ENTRY_SPILL_SIZE 224
#define TRACE_RETURN_SPILL_SIZE 96

.macro TRACE_RESTORE_ENTRY
	ldp    x0, x1, [sp, #0]
	ldp    x2, x3, [sp, #16]
	ldp    x4, x5, [sp, #32]
	ldp    x6, x7, [sp, #48]
	ldp    x8, x9, [sp, #64]
	ldp    q0, q1, [sp, #80]
	ldp    q2, q3, [sp, #112]
	ldp    q4, q5, [sp, #144]
	ldp    q6, q7, [sp, #176]
	ldr    lr, [sp, #208]
	add    sp, sp, #TRACE_ENTRY_SPILL_SIZE
#ifndef _WIN32
	.cfi_restore lr
	.cfi_adjust_cfa_offset -TRACE_ENTRY_SPILL_SIZE
#endif
.endm
#endif

.macro MSGSEND fnname receiver, sel
	EH_START

	cbz    \receiver, 4f                   // Skip everything if the receiver is nil
	                                       // Jump to 6: if this is a small object
	ubfx    x9, \receiver, #0, #SMALLOBJ_BITS
	cbnz   x9, 6f

	ldr    x9, [\receiver]                 // Load class to x9 if not a small int
1:
	ldr    x9, [x9, #DTABLE_OFFSET]        // Dtable -> x9
	ldr    w10, [\sel]                     // selector->index -> x10
	ldr    w11, [x9, #SHIFT_OFFSET]        // dtable->shift -> x11
	
	cmp    x11, #8                         // If this is a small dtable, jump to the
	                                       // small dtable handlers
	b.eq    2f
	cbz    x11, 3f

	ubfx   x11, x10, #16, #8               // Put byte 3 of the sel id in x12
	add    x11, x9, x11, lsl #3            // x11 = dtable address + dtable data offset
	ldr    x9, [x11, #DATA_OFFSET]         // Load, adding in the data offset
2:	                                       // dtable16
	ubfx   x11, x10, #8, #8                // Put byte 2 of the sel id in x12
	add    x11, x9, x11, lsl #3            // x11 = dtable address + dtable data offset
	ldr    x9, [x11, #DATA_OFFSET]         // Load, adding in the data offset
3:                                         // dtable8
	ubfx   x11, x10, #0, #8                // Put low byte of the sel id in x12
	add    x11, x9, x11, lsl #3            // x11 = dtable address + dtable data offset
	ldr    x9, [x11, #DATA_OFFSET]         // Load, adding in the data offset. 
	                                       // Slot pointer is now in x9

	cbz    x9,  5f                         // If the slot is nil, go to the C path

	ldr    x9, [x9, #SLOT_OFFSET]          // Load the method from the slot
7:
#ifdef OBJC2_TRACING_SUPPORTED
	// Tracing is opt-in at build time.  Preserve all AAPCS64 argument
	// registers before consulting the per-selector tracing table.
	sub    sp, sp, #TRACE_ENTRY_SPILL_SIZE
#ifndef _WIN32
	.cfi_adjust_cfa_offset TRACE_ENTRY_SPILL_SIZE
#endif
	stp    x0, x1, [sp, #0]
	stp    x2, x3, [sp, #16]
	stp    x4, x5, [sp, #32]
	stp    x6, x7, [sp, #48]
	stp    x8, x9, [sp, #64]
	stp    q0, q1, [sp, #80]
	stp    q2, q3, [sp, #112]
	stp    q4, q5, [sp, #144]
	stp    q6, q7, [sp, #176]
	str    lr, [sp, #208]
#ifndef _WIN32
	.cfi_offset lr, -16
#endif

	mov    x0, \sel
	bl     CDECL(objc2_tracing_hook_for_selector)
	cbz    x0, 12f
	str    x0, [sp, #216]                 // Preserve tracing hook.

	ldr    x0, [sp, #0]                   // receiver
	ldr    x1, [sp, #8]                   // selector
	ldr    x2, [sp, #72]                  // implementation
	mov    w3, #0                          // entering
	mov    x4, #0                          // no return value on entry
	ldr    x10, [sp, #216]
	blr    x10

	cbz    x0, 13f                        // Entry only: tail-call original IMP.
	cmp    x0, #1
	b.ne   14f                            // Any other value interposes the IMP.

	// Exit tracing needs to call the IMP with the caller's original SP so that
	// stack-passed arguments remain ABI-correct.  Store the tracing context in
	// TLS and keep the original return address in callee-saved x19 while the
	// method runs.
	bl     CDECL(pushTraceReturnStack)
	ldr    x10, [sp, #0]
	str    x10, [x0, #0]                  // receiver
	ldr    x10, [sp, #8]
	str    x10, [x0, #8]                  // selector
	ldr    x10, [sp, #72]
	str    x10, [x0, #16]                 // implementation
	ldr    x10, [sp, #216]
	str    x10, [x0, #24]                 // tracing hook
	ldr    x10, [sp, #208]
	str    x10, [x0, #32]                 // caller return address
	str    x19, [x0, #40]                 // caller x19
	mov    x19, x10

	TRACE_RESTORE_ENTRY
#ifndef _WIN32
	.cfi_register lr, x19
#endif
	blr    x9
15:
	// Preserve all architecturally relevant return registers while the exit
	// hook executes: x0/x1 for integer aggregates and q0-q3 for FP/HFA values.
	sub    sp, sp, #TRACE_RETURN_SPILL_SIZE
#ifndef _WIN32
	.cfi_adjust_cfa_offset TRACE_RETURN_SPILL_SIZE
#endif
	stp    x0, x1, [sp, #0]
	stp    q0, q1, [sp, #16]
	stp    q2, q3, [sp, #48]
	str    x19, [sp, #80]                 // original caller LR
#ifndef _WIN32
	.cfi_offset lr, -16
#endif

	bl     CDECL(popTraceReturnStack)
	str    x0, [sp, #88]
	mov    x11, x0
	ldr    x0, [x11, #0]
	ldr    x1, [x11, #8]
	ldr    x2, [x11, #16]
	mov    w3, #1                          // leaving
	ldr    x4, [sp, #0]                   // word-sized scalar return value
	ldr    x10, [x11, #24]
	blr    x10

	ldr    x11, [sp, #88]
	ldr    x19, [x11, #40]
	ldr    lr, [sp, #80]
	ldp    x0, x1, [sp, #0]
	ldp    q0, q1, [sp, #16]
	ldp    q2, q3, [sp, #48]
	add    sp, sp, #TRACE_RETURN_SPILL_SIZE
#ifndef _WIN32
	.cfi_restore lr
	.cfi_adjust_cfa_offset -TRACE_RETURN_SPILL_SIZE
#endif
	br     lr

12:                                        // No tracing hook for this selector.
	TRACE_RESTORE_ENTRY
	br     x9
13:                                        // Trace entry only.
	TRACE_RESTORE_ENTRY
	br     x9
14:                                        // Interpose with the hook-returned IMP.
	str    x0, [sp, #72]
	TRACE_RESTORE_ENTRY
	br     x9
#else
	br     x9                              // Tail-call the method
#endif

4:	                                       // Nil receiver
	mov    \receiver, #0
	mov    v0.d[0], \receiver
	mov    v0.d[1], \receiver
	br     lr
5:                                        // Slow lookup
	EH_START_AT_OFFSET(\fnname)

	                                      // Save anything that will be clobbered by
	                                      // the call.
										  // Note that we pre-index (see "!"), meaning
										  // that we adjust the sp before storing the pair
										  // of registers.
	stp    x0, x1, [sp, #-(ARGUMENT_SPILL_SIZE)]!
	EH_STACK_ALLOC((ARGUMENT_SPILL_SIZE))

	stp    x2, x3, [sp, #16]
	EH_NOP                                // The following instructions can be ignored by SEH
	stp    x4, x5, [sp, #32]
	EH_NOP
	stp    x6, x7, [sp, #48]
	EH_NOP
	stp    q0, q1, [sp, #64]
	EH_NOP
	stp    q2, q3, [sp, #96]
	EH_NOP
	stp    q4, q5, [sp, #128]
	EH_NOP
	stp    q6, q7, [sp, #160]
	EH_NOP
	stp    fp, lr, [sp, #192]             // The order is arbitrary, except that
	EH_SAVE_FP_LR(192)                    // fp and lr must be spilled together

	add    fp, sp, 192					  // Adjust frame pointer
	EH_ADD_FP(192)
	stp    \receiver, x8, [sp, #-16]!     // it's convenient if \receiver is spilled at sp
	EH_STACK_ALLOC(16)					  // stp performed pre-indexing by sp-16

	EH_END_PROLOGUE

	#ifndef _WIN32
	.cfi_def_cfa fp, 16
	.cfi_offset fp, -16
	.cfi_offset lr, -8
	#endif
	                                      // We now have all argument registers, the link
	                                      // register and the receiver spilled on the
	                                      // stack, with sp containing
	                                      // the address of the receiver

	mov    x0, sp                         // &self, _cmd in arguments
	mov    x1, \sel
	bl     CDECL(slowMsgLookup)           // This is the only place where the EH directives
	                                      // have to be accurate...
	mov    x9, x0                         // IMP -> x9

	EH_START_EPILOGUE
	ldp    x0, x1, [sp, #16]              // Reload spilled argument registers
	EH_NOP
	ldp    x2, x3, [sp, #32]
	EH_NOP
	ldp    x4, x5, [sp, #48]
	EH_NOP
	ldp    x6, x7, [sp, #64]
	EH_NOP
	ldp    q0, q1, [sp, #80]
	EH_NOP
	ldp    q2, q3, [sp, #112]
	EH_NOP
	ldp    q4, q5, [sp, #144]
	EH_NOP
	ldp    q6, q7, [sp, #176]
	EH_NOP
	ldp    fp, lr, [sp, #208]
	EH_SAVE_FP_LR(208)

										  // Post-increment sp += ARGUMENT_SPILL_SIZE +16
	ldp    \receiver, x8, [sp], #(ARGUMENT_SPILL_SIZE + 16)
	EH_STACK_ALLOC((ARGUMENT_SPILL_SIZE + 16))

	EH_END_EPILOGUE
	EH_END_AT_OFFSET(\fnname)

	b      7b
6:
										  // Load 63:12 of SmallObjectClasses address
										  // We use the CDECL macro as Windows prefixes
										  // cdecl conforming symbols with "_".
#ifdef __APPLE__
	adrp   x10, CDECL(SmallObjectClasses)@PAGE
	add    x10, x10, CDECL(SmallObjectClasses)@PAGEOFF
#else
	adrp   x10, CDECL(SmallObjectClasses) // The macro handles this transparently.
	add    x10, x10, :lo12:CDECL(SmallObjectClasses)
#endif
	ldr    x9, [x10, x9, lsl #3]

	b      1b
	EH_END
.endm

.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), %function)
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), %function)
.globl CDECL(objc_msgSend_stret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret), %function)
CDECL(objc_msgSend):
CDECL(objc_msgSend_fpret):
CDECL(objc_msgSend_stret):
	MSGSEND objc_msgSend, x0, x1

/*
  In AAPCS, an SRet is passed in x8, not x0 like a normal pointer parameter.
  On Windows, this is only the case for POD (plain old data) types. Non trivial
  types with constructors and destructors are passed in x0 on sret.

  We thus need two objc_msgSend functions on Windows on ARM64 for Sret:
  	1. objc_msgSend_stret for POD Sret
	2. objc_msgSend_stret2 for non-trivial Sret (like C++ class instances)
 */
#ifdef _WIN32
.globl CDECL(objc_msgSend_stret2)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret2), %function)
CDECL(objc_msgSend_stret2):
	MSGSEND objc_msgSend_stret2, x1, x2

.text
.def objc_msgSend;
.scl 2;
.type 32;
.endef
.def objc_msgSend_fpret;
.scl 2;
.type 32;
.endef
.def objc_msgSend_stret;
.scl 2;
.type 32;
.endef
.def objc_msgSend_stret2;
.scl 2;
.type 32;
.endef

.section        .drectve,"yn"
.ascii  " /EXPORT:objc_msgSend"
.ascii  " /EXPORT:objc_msgSend_fpret"
.ascii  " /EXPORT:objc_msgSend_stret"
.ascii  " /EXPORT:objc_msgSend_stret2"
#endif