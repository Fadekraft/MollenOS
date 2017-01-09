/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * MollenOS x86 Common Threading Interface
 * - Contains shared x86 threading routines
 */

/* Includes 
 * - System */
#include <arch.h>
#include <threading.h>
#include <process/phoenix.h>
#include <thread.h>
#include <memory.h>
#include <heap.h>
#include <apic.h>
#include <gdt.h>
#include <log.h>

/* Includes
 * - Library */
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Extern access, we need access
 * to the timer-quantum and a bunch of 
 * assembly functions */
__CRT_EXTERN size_t GlbTimerQuantum;
__CRT_EXTERN void save_fpu(Addr_t *buffer);
__CRT_EXTERN void set_ts(void);
__CRT_EXTERN void _yield(void);
__CRT_EXTERN void enter_thread(Registers_t *Regs);
__CRT_EXTERN void enter_signal(Registers_t *Regs, Addr_t Handler, int Signal, Addr_t Return);
__CRT_EXTERN void RegisterDump(Registers_t *Regs);

/* The yield interrupt code for switching tasks
 * and is controlled by software interrupts, the yield interrupt
 * also like the apic switch need to reload the apic timer as it
 * controlls the primary switch */
int ThreadingYield(void *Args)
{
	/* Variables we will need for loading
	 * a new task */
	Registers_t *Regs = NULL;
	Cpu_t CurrCpu = ApicGetCpu();

	/* These will be assigned from the 
	 * _switch function, but set them in
	 * case threading is not initialized yet */
	size_t TimeSlice = 20;
	int TaskPriority = 0;

	/* Before we do anything, send EOI so 
	 * we don't forget :-) */
	ApicSendEoi(APIC_NO_GSI, INTERRUPT_YIELD);

	/* Switch Task, if there is no threading enabled yet
	 * it should return the same structure as we give */
	Regs = _ThreadingSwitch((Registers_t*)Args, 0, &TimeSlice, &TaskPriority);

	/* If we just got hold of idle task, well fuck it disable timer 
	 * untill we get another task */
	if (!ThreadingIsCurrentTaskIdle(CurrCpu)) {
		ApicSetTaskPriority(61 - TaskPriority);
		ApicWriteLocal(APIC_INITIAL_COUNT, GlbTimerQuantum * TimeSlice);
	}
	else {
		ApicSetTaskPriority(0);
		ApicWriteLocal(APIC_INITIAL_COUNT, 0);
	}

	/* Enter new thread */
	enter_thread(Regs);

	/* Never reached */
	return X86_IRQ_HANDLED;
}

/* Initialization 
 * Creates the main thread */
void *IThreadInitBoot(void)
{
	/* Vars */
	x86Thread_t *Init;

	/* Setup initial thread */
	Init = (x86Thread_t*)kmalloc(sizeof(x86Thread_t));
	memset(Init, 0, sizeof(x86Thread_t));

	Init->FpuBuffer = kmalloc_a(0x1000);
	Init->Flags = X86_THREAD_FPU_INITIALISED | X86_THREAD_USEDFPU;

	/* Memset the buffer */
	memset(Init->FpuBuffer, 0, 0x1000);

	/* Install Yield */
	InterruptInstallIdtOnly(APIC_NO_GSI, INTERRUPT_YIELD, ThreadingYield, NULL);

	/* Done */
	return Init;
}

/* Initialises AP task */
void *IThreadInitAp(void)
{
	/* Vars */
	x86Thread_t *Init;

	/* Setup initial thread */
	Init = (x86Thread_t*)kmalloc(sizeof(x86Thread_t));
	memset(Init, 0, sizeof(x86Thread_t));

	Init->FpuBuffer = kmalloc_a(0x1000);
	Init->Flags = X86_THREAD_FPU_INITIALISED | X86_THREAD_USEDFPU;

	/* Memset the buffer */
	memset(Init->FpuBuffer, 0, 0x1000);

	/* Done */
	return Init;
}

/* Wake's up CPU */
void IThreadWakeCpu(Cpu_t Cpu)
{
	/* Send an IPI to the cpu */
	ApicSendIpi(Cpu, INTERRUPT_YIELD);
}

/* Yield current thread */
_CRT_EXPORT void IThreadYield(void)
{
	/* Call the extern */
	_yield();
}

/* Create a new thread */
void *IThreadInit(Addr_t EntryPoint)
{
	/* Vars */
	x86Thread_t *t;
	Cpu_t Cpu;

	/* Get cpu */
	Cpu = ApicGetCpu();

	/* Allocate a new thread structure */
	t = (x86Thread_t*)kmalloc(sizeof(x86Thread_t));
	memset(t, 0, sizeof(x86Thread_t));

	/* Setup */
	t->Context = ContextCreate(THREADING_KERNELMODE, (Addr_t)EntryPoint, 0, NULL);

	/* FPU */
	t->FpuBuffer = (Addr_t*)kmalloc_a(0x1000);

	/* Memset the buffer */
	memset(t->FpuBuffer, 0, 0x1000);

	/* Done */
	return t;
}

/* Frees thread resources */
void IThreadDestroy(void *ThreadData)
{
	/* Cast */
	x86Thread_t *Thread = (x86Thread_t*)ThreadData;

	/* Cleanup Contexts */
	kfree(Thread->Context);

	/* Not all has user context */
	if (Thread->UserContext != NULL)
		kfree(Thread->UserContext);

	/* Free fpu buffer */
	kfree(Thread->FpuBuffer);

	/* Free structure */
	kfree(Thread);
}

/* Setup Usermode */
void IThreadInitUserMode(void *ThreadData, 
	Addr_t StackAddr, Addr_t EntryPoint, Addr_t ArgumentAddress)
{
	/* Cast */
	x86Thread_t *t = (x86Thread_t*)ThreadData;

	/* Create user-context */
	t->UserContext = ContextCreate(THREADING_USERMODE, 
		EntryPoint, StackAddr, (Addr_t*)ArgumentAddress);

	/* Disable all port-access */
	memset(&t->IoMap[0], 0xFF, GDT_IOMAP_SIZE);
}

/* Dispatches a signal to the given process 
 * signals will always be dispatched to main thread */
void SignalDispatch(MCoreAsh_t *Ash, MCoreSignal_t *Signal)
{
	/* Variables */
	MCoreThread_t *Thread = ThreadingGetThread(Ash->MainThread);
	x86Thread_t *Thread86 = (x86Thread_t*)Thread->ThreadData;
	Registers_t *Regs = NULL;

	/* User or kernel mode thread? */
	if (Thread->Flags & THREADING_USERMODE)
		Regs = Thread86->UserContext;
	else
		Regs = Thread86->Context;

	/* Store current context */
	memcpy(&Signal->Context, Regs, sizeof(Registers_t));

	/* Now we can enter the signal context 
	 * handler, we cannot return from this function */
	enter_signal(Regs, Signal->Handler, Signal->Signal, MEMORY_LOCATION_SIGNAL_RET);
}

/* Task Switch occurs here */
Registers_t *_ThreadingSwitch(Registers_t *Regs, int PreEmptive, size_t *TimeSlice, 
							 int *TaskQueue)
{
	/* We'll need these */
	MCoreThread_t *mThread;
	x86Thread_t *tx86;
	Cpu_t Cpu;

	/* Sanity */
	if (ThreadingIsEnabled() != 1)
		return Regs;

	/* Get CPU */
	Cpu = ApicGetCpu();

	/* Get thread */
	mThread = ThreadingGetCurrentThread(Cpu);

	/* What the fuck?? */
	assert(mThread != NULL && Regs != NULL);

	/* Cast */
	tx86 = (x86Thread_t*)mThread->ThreadData;

	/* Save FPU/MMX/SSE State */
	if (tx86->Flags & X86_THREAD_USEDFPU)
		save_fpu(tx86->FpuBuffer);

	/* Save stack */
	if (mThread->Flags & THREADING_USERMODE)
		tx86->UserContext = Regs;
	else
		tx86->Context = Regs;

	/* Switch */
	mThread = ThreadingSwitch(Cpu, mThread, (uint8_t)PreEmptive);
	tx86 = (x86Thread_t*)mThread->ThreadData;

	/* Update user variables */
	*TimeSlice = mThread->TimeSlice;
	*TaskQueue = mThread->Queue;

	/* Update Addressing Space */
	MmVirtualSwitchPageDirectory(Cpu, 
		(PageDirectory_t*)mThread->AddressSpace->PageDirectory, mThread->AddressSpace->Cr3);

	/* Set TSS */
	TssUpdateStack(Cpu, (Addr_t)tx86->Context);
	TssUpdateIo(Cpu, &tx86->IoMap[0]);

	/* Finish Transition */
	if (mThread->Flags & THREADING_TRANSITION) {
		mThread->Flags &= ~THREADING_TRANSITION;
		mThread->Flags |= THREADING_USERMODE;
	}

	/* Clear FPU/MMX/SSE */
	tx86->Flags &= ~X86_THREAD_USEDFPU;

	/* Handle signals if any */
	SignalHandle(mThread->Id);

	/* Set TS bit in CR0 */
	set_ts();

	/* Return new stack */
	if (mThread->Flags & THREADING_USERMODE)
		return tx86->UserContext;
	else
		return tx86->Context;
}