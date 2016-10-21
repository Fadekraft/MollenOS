/* MollenOS
*
* Copyright 2011 - 2016, Philip Meulengracht
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
* MollenOS InterProcess Comm Interface
*/

#ifndef _MOLLENOS_IPC_H_
#define _MOLLENOS_IPC_H_

/* Includes */
#include <os/osdefs.h>
#include <os/virtualkeycodes.h>

/***********************
 * Base IPC Message Type
 ***********************/
typedef enum _MEventType
{
	EventGeneric,
	EventInput,

	/* Server Events */
	EventServerControl,
	EventServerCommand,
	EventServerResponse

} MEventType_t;

/***********************
 * Generic IPC Message Type
 ***********************/
typedef enum _MGenericMessageType
{
	/* Window Manager 
	 * - Control Messages */
	GenericWindowCreate,
	GenericWindowDestroy,
	GenericWindowInvalidate,
	GenericWindowQuery,

	/* Servers 
	 * - Control Messages */
	GenericServerPing,
	GenericServerPong,
	GenericServerRestart,
	GenericServerQuit

} MGenericMessageType_t;

/***********************
 * Input IPC Message Type
 ***********************/
typedef enum _MInputSourceType
{
	InputUnknown = 0,
	InputMouse,
	InputKeyboard,
	InputKeypad,
	InputJoystick,
	InputGamePad,
	InputOther

} MInputSourceType_t;

/*********************** 
 * Base IPC Message 
 ***********************/
typedef struct _MEventMessageBase
{
	/* Message Type */
	MEventType_t Type;

	/* Message Length 
	 * Including this base header */
	size_t Length;

	/* Message Source 
	 * The sender of this message */
	IpcComm_t Sender;

} MEventMessageBase_t;

/***********************
 * Generic IPC Message
 ***********************/
typedef struct _MEventMessageGeneric
{
	/* Base */
	MEventMessageBase_t Header;

	/* Message Type */
	MGenericMessageType_t Type;

	/* Param 1 */
	size_t LoParam;

	/* Param 2 */
	size_t HiParam;

	/* Param Rect */
	Rect_t RcParam;

} MEventMessageGeneric_t;

/***********************
 * Input IPC Message
 *  - Button Event
 ***********************/
typedef struct _MEventMessageInput
{
	/* Header */
	MEventMessageBase_t Header;

	/* Input Type */
	MInputSourceType_t Type;

	/* Button Data (Keycode / Symbol) */
	unsigned Scancode;
	VKey Key;

	/* Flags (Bit-field, see under structure) */
	unsigned Flags;

	/* Axis Data
	 * Must be relative */
	ssize_t xRelative;
	ssize_t yRelative;
	ssize_t zRelative;

	/* Rotation Data */

} MEventMessageInput_t; 

/* Flags - Event Types */
#define MCORE_INPUT_BUTTON_RELEASED		0x0
#define MCORE_INPUT_BUTTON_CLICKED		0x1
#define MCORE_INPUT_MULTIPLEKEYS		0x2

/***********************
* Input IPC Message
***********************/
typedef union _MEventMessage
{
	/* Base Message
	 * Always use this to determine 
	 * which structure to access */
	MEventMessageBase_t Base;

	/* Generic Message 
	 * Used for pretty much any
	 * message passed between processes */
	MEventMessageGeneric_t Generic;

	/* Events, these are driver messages
	 * that rely on static message space
	 * Contain data as mouse position, 
	 * button states etc */
	MEventMessageInput_t Input;

} MEventMessage_t;

#endif //!_MOLLENOS_IPC_H_
