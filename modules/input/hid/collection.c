/**
 * MollenOS
 *
 * Copyright 2017, Philip Meulengracht
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
 * Human Input Device Driver (Generic)
 */

#define __TRACE

#include "hid.h"
#include <usb/usb.h>
#include <ddk/utils.h>
#include <stdlib.h>

#include <ctt_input_protocol_server.h>

/**
 * Retrieves a value from a buffer by the given bit-offset and the for a certain
 * number of bits, the extracted value will be treated unsigned
 */
static uint64_t __ExtractValue(
    _In_ const uint8_t* buffer,
    _In_ uint32_t       bitOffset,
    _In_ uint32_t       numBits)
{
    uint64_t value = 0;
    uint32_t i = 0;
    uint32_t offset = bitOffset;

    while (i < numBits) {
        uint8_t bits = ((offset % 8) + numBits - i) < 8 ? numBits % 8 : 8 - (offset % 8);
        value |= ((buffer[offset / 8] >> (offset % 8)) & ((1 << bits) - 1)) << i;
        i += bits;
        offset += bits;
    }
    return value;
}

static UsbHidReportCollection_t* __CreateCollection(
    _In_ UsbHidReportGlobalStats_t* globalState,
    _In_ UsbHidReportItemStats_t*   itemState)
{
    UsbHidReportCollection_t* collection;
    TRACE("__CreateCollection()");

    collection = (UsbHidReportCollection_t*)malloc(sizeof(UsbHidReportCollection_t));
    if (!collection) {
        return NULL;
    }

    collection->UsagePage = globalState->UsagePage;
    collection->Usage     = itemState->Usages[0];
    collection->Childs    = NULL;
    collection->Parent    = NULL;
    return collection;
}

void __CreateCollectionChild(
    _In_ UsbHidReportCollection_t*  reportCollection,
    _In_ UsbHidReportGlobalStats_t* globalStats,
    _In_ DeviceInputType_t          inputType,
    _In_ int                        collectionType,
    _In_ void*                      item)
{
    UsbHidReportCollectionItem_t* child;
    TRACE("__CreateCollectionChild(inputType=%u, collectionType=%i)", inputType, collectionType);

    child = (UsbHidReportCollectionItem_t*)malloc(sizeof(UsbHidReportCollectionItem_t));
    if (!child) {
        ERROR("__CreateCollectionChild child was null");
        return;
    }

    child->CollectionType = collectionType;
    child->InputType      = inputType;
    child->ItemPointer    = item;
    child->Link           = NULL;

    // Make a local copy of the global active stats
    memcpy(&child->Stats, globalStats, sizeof(UsbHidReportGlobalStats_t));

    // Either insert at root if childs are null or insert at end
    if (reportCollection->Childs == NULL) {
        reportCollection->Childs = child;
    }
    else {
        UsbHidReportCollectionItem_t* currentChild = reportCollection->Childs;
        while (currentChild->Link) {
            currentChild = currentChild->Link;
        }
        currentChild->Link = child;
    }
}

void __ParseGlobalStateTag(
    _In_ UsbHidReportGlobalStats_t* globalStats,
    _In_ uint8_t                    tag,
    _In_ uint32_t                   value)
{
    TRACE("__ParseGlobalStateTag(tag=%x, value=0x%x)", tag, value);
    switch (tag) {
        // The usage page is the most frequently data appearing in collections
        // they describe the type of device.
        case HID_GLOBAL_USAGE_PAGE: {
            globalStats->UsagePage = value;
        } break;

        // The logical minimum setting, describes the
        // lowest logical value to expect
        case HID_GLOBAL_LOGICAL_MIN: {
            // Detect new pairs of logical-min/max
            if (globalStats->HasLogicalMin != 0) {
                globalStats->HasLogicalMax = 0;
            }

            // Store the value and mark its presence
            globalStats->LogicalMin    = (int32_t)value;
            globalStats->HasLogicalMin = 1;

            // If we have it's counter-part we have to sanitize
            // it's value, because if low-part is higher/equal to
            // higher part, then we have to negate the value by
            // signing it. (It means the low-part is negative)
            if (globalStats->HasLogicalMax != 0) {
                if ((int)(globalStats->LogicalMin) >= (int)(globalStats->LogicalMax)) {
                    globalStats->LogicalMin = ~(globalStats->LogicalMin);
                    globalStats->LogicalMin++;
                }
            }
        } break;

        // The logical maximum setting, describes the
        // highest logical value to expect
        case HID_GLOBAL_LOGICAL_MAX: {
            // Detect new pairs of logical-min/max
            if (globalStats->HasLogicalMax != 0) {
                globalStats->HasLogicalMin = 0;
            }

            // Store the value and mark its presence
            globalStats->LogicalMax    = (int32_t)value;
            globalStats->HasLogicalMax = 1;

            // If we have it's counter-part we have to sanitize
            // it's value, because if low-part is higher/equal to
            // higher part, then we have to negate the value by
            // signing it. (It means the low-part is negative)
            if (globalStats->HasLogicalMin != 0) {
                if ((int)(globalStats->LogicalMin) >= (int)(globalStats->LogicalMax)) {
                    globalStats->LogicalMin = ~(globalStats->LogicalMin);
                    globalStats->LogicalMin++;
                }
            }
        } break;

        // The physical minimum setting, describes the
        // lowest phyiscal value to expect
        case HID_GLOBAL_PHYSICAL_MIN: {
            // Detect new pairs of physical-min/max
            if (globalStats->HasPhysicalMin != 0) {
                globalStats->HasPhysicalMax = 0;
            }

            // Store the value and mark its presence
            globalStats->PhysicalMin    = (int32_t)value;
            globalStats->HasPhysicalMin = 1;

            // If we have it's counter-part we have to sanitize
            // it's value, because if low-part is higher/equal to
            // higher part, then we have to negate the value by
            // signing it. (It means the low-part is negative)
            if (globalStats->HasPhysicalMax != 0) {
                if ((int)(globalStats->PhysicalMin) >= (int)(globalStats->PhysicalMax)) {
                    globalStats->PhysicalMin = ~(globalStats->PhysicalMin);
                    globalStats->PhysicalMin++;
                }
            }
        } break;

        // The physical maximum setting, describes the
        // highest phyiscal value to expect
        case HID_GLOBAL_PHYSICAL_MAX: {
            // Detect new pairs of physical-min/max
            if (globalStats->HasPhysicalMax != 0) {
                globalStats->HasPhysicalMin = 0;
            }

            // Store the value and mark its presence
            globalStats->PhysicalMax    = (int32_t)value;
            globalStats->HasPhysicalMax = 1;

            // If we have it's counter-part we have to sanitize
            // it's value, because if low-part is higher/equal to
            // higher part, then we have to negate the value by
            // signing it. (It means the low-part is negative)
            if (globalStats->HasPhysicalMin != 0) {
                if ((int)(globalStats->PhysicalMin) >= (int)(globalStats->PhysicalMax)) {
                    globalStats->PhysicalMin = ~(globalStats->PhysicalMin);
                    globalStats->PhysicalMin++;
                }
            }
        } break;

        // The Unit & Unit Exponent describes the type of data-unit
        // we can expect from the device, or this given input/output.
        // and if there is an exponent attached we need that for the calculations
        case HID_GLOBAL_UNIT_VALUE: {
            globalStats->UnitType = (int32_t)value;
        } break;
        case HID_GLOBAL_UNIT_EXPONENT: {
            globalStats->UnitExponent = (int32_t)value;
        } break;

        // Report items like Id, Count and Size tells about the actual
        // report that we recieve from the device under data-transfers.
        // These must be present if the device can send us reports. (Except Id)
        case HID_GLOBAL_REPORT_ID: {
            globalStats->ReportId = value;
        } break;
        case HID_GLOBAL_REPORT_COUNT: {
            globalStats->ReportCount = value;
        } break;
        case HID_GLOBAL_REPORT_SIZE: {
            globalStats->ReportSize = value;
        } break;

        // If there is anything we don't handle it's not vital
        // but we should trace it in case we want to handle it
        default: {
            TRACE("Global Item %u", tag);
        } break;
    }
}

struct ReportParserContext {
    UsbHidReportCollection_t* CurrentCollection;
    UsbHidReportCollection_t* RootCollection;
    UsbHidReportGlobalStats_t GlobalStats;
    UsbHidReportItemStats_t   ItemStats;

    DeviceInputType_t         InputType;
    int                       ParseDepth;
    int                       ReportIdsUsed;
    size_t                    LongestReport;
    size_t                    BitOffset;
};

static void __ParseReportTagMainCollection(
        _In_ struct ReportParserContext* context)
{
    UsbHidReportCollection_t* collection;
    TRACE("__ParseReportTagMainCollection()");

    // Create a collection from the current state-variables
    collection = __CreateCollection(&context->GlobalStats, &context->ItemStats);
    if (!collection) {
        ERROR("__ParseReportTagMainCollection collection is null");
        return;
    }

    // Set it if current is not set
    // then we don't need to insert it to list
    if (!context->CurrentCollection) {
        context->CurrentCollection = collection;
    }
    else {
        // Update the parent of the collection
        collection->Parent = context->CurrentCollection;

        // Append it as a child note now that we
        // aren't a child
        __CreateCollectionChild(context->CurrentCollection, &context->GlobalStats, context->InputType,
                HID_TYPE_COLLECTION, collection);

        // Step into new collection
        context->CurrentCollection = collection;
    }

    // Note the current depth
    context->ParseDepth++;
}

static void __ParseReportTagMainCollectionEnd(
        _In_ struct ReportParserContext* context)
{
    TRACE("__ParseReportTypeMain()");
    if (context->CurrentCollection != NULL) {
        // If we finish with root collection, no more!
        if (context->CurrentCollection->Parent == NULL) {
            context->RootCollection = context->CurrentCollection;
        }
        context->CurrentCollection = context->CurrentCollection->Parent;
    }

    // Note the current depth
    context->ParseDepth--;
}

static void __ParseReportTagInput(
        _In_ struct ReportParserContext* context,
        _In_ uint32_t                    packet)
{
    UsbHidReportInputItem_t* inputItem;
    TRACE("__ParseReportTypeMain(packet=0x%x)", packet);

    inputItem = (UsbHidReportInputItem_t*)malloc(sizeof(UsbHidReportInputItem_t));
    if (!inputItem) {
        ERROR("__ParseReportTagInput inputItem is null");
        return;
    }

    // If the constant bit is set it overrides rest
    // of the bits.
    if (packet & 0x1) {
        inputItem->Flags = REPORT_INPUT_TYPE_CONSTANT;
    }
    else {
        // Ok, so the data available is actual dynamic data, not constant data
        // Now determine if its variable data or array data
        if (packet & 0x2) {
            // If bit 2 is set, the data is variable and relative,
            // otherwise the data is variable but absolute
            if (packet & 0x4) {
                inputItem->Flags = REPORT_INPUT_TYPE_RELATIVE;
            }
            else {
                inputItem->Flags = REPORT_INPUT_TYPE_ABSOLUTE;
            }
        }
        else {
            inputItem->Flags = REPORT_INPUT_TYPE_ARRAY;
        }
    }

    // Debug
    TRACE("Input type %u at bit-offset %u, with data-size in bits %u",
          inputItem->Flags, context->BitOffset,
          (context->GlobalStats.ReportCount * context->GlobalStats.ReportSize));

    // Create a new copy of the current local state that applies
    // only to this input item. Override BitOffset member
    memcpy(&inputItem->LocalState, &context->ItemStats, sizeof(UsbHidReportItemStats_t));
    inputItem->LocalState.BitOffset = context->BitOffset;

    // Append it as a child note now that we aren't a child
    __CreateCollectionChild(context->CurrentCollection, &context->GlobalStats, context->InputType,
            HID_TYPE_INPUT, inputItem);

    // Adjust BitOffset now to past this item
    // and also sanitize current length, make sure we store the longest report
    context->BitOffset += context->GlobalStats.ReportCount * context->GlobalStats.ReportSize;
    if ((context->GlobalStats.ReportCount * context->GlobalStats.ReportSize) > context->LongestReport) {
        context->LongestReport = context->GlobalStats.ReportCount * context->GlobalStats.ReportSize;
    }
}

static void __ParseReportTypeMain(
        _In_ struct ReportParserContext* context,
        _In_ uint8_t                     tag,
        _In_ uint32_t                    packet)
{
    int i;
    TRACE("__ParseReportTypeMain(tag=%x, packet=0x%x)", tag, packet);

    switch (tag) {
        // The collection contains a number of collection items
        // which can be anything. They usually describe a different
        // kind of report-collection which means the input device
        // has a number of configurations
        case HID_MAIN_COLLECTION: {
            __ParseReportTagMainCollection(context);
        } break;

            // The end of collection marker means our collection is
            // closed and we should switch to parent collection context
        case HID_MAIN_ENDCOLLECTION: {
            __ParseReportTagMainCollectionEnd(context);
        } break;

            // Input items can describe any kind of physical input device
            // as mouse pointers, buttons, joysticks etc. The type of data
            // we recieve is described as either Constant, Relative, Absolute or Array
        case HID_MAIN_INPUT: {
            __ParseReportTagInput(context, packet);
        } break;

            // Output examples could be @todo
        case HID_MAIN_OUTPUT: {

        } break;

            // Feature examples could be @todo
        case HID_MAIN_FEATURE: {

        } break;

        default:  {
            break;
        }
    }

    // At the end of a collection item we need to reset the local collection item stats
    for (i = 0; i < 16; i++) {
        context->ItemStats.Usages[i] = 0;
    }
    context->ItemStats.UsageMin  = 0;
    context->ItemStats.UsageMax  = 0;
    context->ItemStats.BitOffset = 0;
}

static void __ParseReportTypeLocal(
        _In_ struct ReportParserContext* context,
        _In_ uint8_t                     tag,
        _In_ uint32_t                    packet)
{
    TRACE("__ParseReportTypeLocal(tag=%x, packet=0x%x)", tag, packet);
    switch (tag) {
        // The usage tag describes which kind of device we are dealing
        // with and are usefull for determing how to handle it.
        case HID_LOCAL_USAGE: {
            int j;

            // Determine the kind of input device
            if (packet == HID_REPORT_USAGE_POINTER || packet == HID_REPORT_USAGE_MOUSE) {
                context->InputType = DeviceInputPointer;
            }
            else if (packet == HID_REPORT_USAGE_KEYBOARD) {
                context->InputType = DeviceInputKeyboard;
            }
            else if (packet == HID_REPORT_USAGE_KEYPAD) {
                context->InputType = DeviceInputKeypad;
            }
            else if (packet == HID_REPORT_USAGE_JOYSTICK) {
                context->InputType = DeviceInputJoystick;
            }
            else if (packet == HID_REPORT_USAGE_GAMEPAD) {
                context->InputType = DeviceInputGamePad;
            }

            // There can be multiple usages for an descriptor
            // so store up to 16 usages
            for (j = 0; j < 16; j++) {
                if (context->ItemStats.Usages[j] == 0) {
                    context->ItemStats.Usages[j] = packet;
                    break;
                }
            }
        } break;

            // The usage min and max tells us the boundaries of
            // the data package
        case HID_LOCAL_USAGE_MIN: {
            context->ItemStats.UsageMin = packet;
        } break;
        case HID_LOCAL_USAGE_MAX: {
            context->ItemStats.UsageMax = packet;
        } break;

            // If there is anything we don't handle it's not vital
            // but we should trace it in case we want to handle it
        default: {
            TRACE("%u: Local Item %u", context->ParseDepth, tag);
        } break;
    }
}

static void __ParsePacket(
        _In_ struct ReportParserContext* context,
        _In_ uint8_t                     type,
        _In_ uint8_t                     tag,
        _In_ uint32_t                    packet)
{
    TRACE("__ParsePacket(type=%x, tag=%x, packet=0x%x)", type, tag, packet);
    // The first item that appears in type main MUST be collection otherwise just skip
    if (!context->CurrentCollection && type == HID_REPORT_TYPE_MAIN && tag != HID_MAIN_COLLECTION) {
        return;
    }

    // The type we encounter can be of these types:
    // Main, Global, Local or LongItem
    switch (type) {
        // Main items describe an upper container of either inputs, outputs
        // features or a collection of items (Tag variable)
        case HID_REPORT_TYPE_MAIN: {
            __ParseReportTypeMain(context, tag, packet);
        } break;

            // Global items are actually a global state for the entire collection
            // and contains settings that are applied to all children elements
            // They can also carry a report-id which means they only apply to a given
            // report
        case HID_REPORT_TYPE_GLOBAL: {
            __ParseGlobalStateTag(&context->GlobalStats, tag, packet);
            if (context->GlobalStats.ReportId != UUID_INVALID) {
                context->ReportIdsUsed = 1;
            }
        } break;

            // Local items are a local state that only applies to items in the current
            // collection, and thus are reset between major items.
        case HID_REPORT_TYPE_LOCAL: {
            __ParseReportTypeLocal(context, tag, packet);
        } break;

        default:  {
            break;
        }
    }
}

size_t
HidParseReportDescriptor(
    _In_ HidDevice_t*   hidDevice,
    _In_ const uint8_t* descriptor,
    _In_ size_t         descriptorLength)
{
    struct ReportParserContext context = { 0 };
    size_t                     i;
    TRACE("HidParseReportDescriptor(hidDevice=0x%" PRIxIN ", descriptorLength=0x%" PRIuIN ")",
          hidDevice, descriptorLength);

    // Make sure we set the report id to not available
    context.GlobalStats.ReportId = UUID_INVALID;
    context.InputType = DeviceInputPointer;

    // Iterate the report descriptor
    for (i = 0; i < descriptorLength; /* Increase manually */) {
        // Bits 0-1 (Must be either 0, 1, 2 or 4) 3 = 4
        uint8_t  packetLength = descriptor[i] & 0x03;
        uint8_t  type         = descriptor[i] & 0x0C; // Bits 2-3
        uint8_t  tag          = descriptor[i] & 0xF0; // Bits 4-7
        uint32_t packet       = 0;

        // Sanitize size, if 3, it must be 4
        if (packetLength == 3) {
            packetLength++;
        }

        // Get actual packet (The byte(s) after the header)
        if (packetLength == 1) {
            packet = descriptor[i + 1];
        }
        else if (packetLength == 2) {
            packet = descriptor[i + 1]
                     | (uint32_t)((descriptor[i + 2] << 8) & 0xFF00);
        }
        else if (packetLength == 4) {
            packet = descriptor[i + 1]
                     | (uint32_t)((descriptor[i + 2] << 8) & 0xFF00)
                     | (uint32_t)((descriptor[i + 3] << 16) & 0xFF0000)
                     | (uint32_t)((descriptor[i + 4] << 24) & 0xFF000000);
        }

        // Update Report Pointer
        i += (packetLength + 1);

        // Parse packet
        __ParsePacket(&context, type, tag, packet);
    }

    // Store the collection in the device
    // and return the calculated number of maximum bytes reports can use
    hidDevice->Collection = (context.RootCollection == NULL) ? context.CurrentCollection : context.RootCollection;
    if (context.ReportIdsUsed) {
        return DIVUP(context.LongestReport, 8) + 1;
    }
    else {
        return DIVUP(context.BitOffset, 8);
    }
}

OsStatus_t
HidCollectionDestroy(
    _In_ UsbHidReportCollection_t* reportCollection)
{
    UsbHidReportCollectionItem_t *ChildIterator = NULL, *Temporary = NULL;

    if (!reportCollection) {
        return OsInvalidParameters;
    }

    // Iterate it's children
    ChildIterator = reportCollection->Childs;
    while (ChildIterator != NULL) {
        switch (ChildIterator->CollectionType) {
            case HID_TYPE_COLLECTION: {
                HidCollectionDestroy((UsbHidReportCollection_t*)ChildIterator->ItemPointer);
            } break;
            default: {
                free(ChildIterator->ItemPointer);
            }
        }

        // Switch link, free iterator
        Temporary = ChildIterator;
        ChildIterator = ChildIterator->Link;
        free(Temporary);
    }

    // Last step is to free the given collection
    free(reportCollection);
    return OsSuccess;
}

OsStatus_t
HidCollectionCleanup(
    _In_ HidDevice_t* hidDevice)
{
    if (!hidDevice) {
        return OsInvalidParameters;
    }
    return HidCollectionDestroy(hidDevice->Collection);
}

void
HidParseReportInput(
    _In_ HidDevice_t*                  hidDevice,
    _In_ UsbHidReportCollectionItem_t* collectionItem,
    _In_ size_t                        dataIndex)
{
    size_t                   i;
    size_t                   offset;
    size_t                   length;
    UsbHidReportInputItem_t* inputItem;
    uint8_t*                 previousData;
    uint8_t*                 data;

    union {
        struct ctt_input_cursor_event inputData;
        struct ctt_input_button_event buttonEvent;
    } eventData = { { 0 } };

    // Cast the input-item from the ItemPointer
    inputItem = (UsbHidReportInputItem_t*)collectionItem->ItemPointer;

    // Initiate pointers to new and previous data
    data         = &((uint8_t*)hidDevice->Buffer)[dataIndex];
    previousData = &((uint8_t*)hidDevice->Buffer)[hidDevice->PreviousDataIndex];

    // Sanitize the type of input, if we are constant, it's padding
    if (inputItem->Flags == REPORT_INPUT_TYPE_CONSTANT) {
        return;
    }

    // If report-ids are active, we must make sure this data-packet
    // is actually for this report
    // The first byte of the data-report is the id
    if (collectionItem->Stats.ReportId != UUID_INVALID) {
        uint8_t reportId = data[0];
        if (reportId != (uint8_t)collectionItem->Stats.ReportId) {
            return;
        }
    }

    // Extract some of the state variables for parsing
    offset = inputItem->LocalState.BitOffset;
    length = collectionItem->Stats.ReportSize;

    // Iterate the data
    for (i = 0; i < collectionItem->Stats.ReportCount; i++, offset += length) {
        uint64_t value    = __ExtractValue(data, offset, length);
        uint64_t oldValue = __ExtractValue(previousData, offset, length);
        
        // We cant expect this to be correct though, it might be 0
        int usage = inputItem->LocalState.Usages[i];

        // Take action based on the type of input
        // currently we only handle generic pc input devices
        switch (collectionItem->Stats.UsagePage) {
            case HID_USAGE_PAGE_GENERIC_PC: {
                switch (usage) {
                    // Calculating Device Bounds 
                    // Resolution = (Logical Maximum � Logical Minimum) / 
                    // ((Physical Maximum � Physical Minimum) * (10 Unit Exponent))

                    // If physical min/max is not defined or are 0, 
                    // we set them to be logical min/max

                    // Grid updates like x, y or z coordinates have changed.
                    case HID_REPORT_USAGE_X_AXIS:
                    case HID_REPORT_USAGE_Y_AXIS:
                    case HID_REPORT_USAGE_Z_AXIS: {
                        int64_t relativeValue = (int64_t)value;
                        if (value == 0) {
                            break;
                        }

                        // If the value is absolute, we want to
                        // make sure we calculate the relative
                        if (inputItem->Flags == REPORT_INPUT_TYPE_ABSOLUTE) {
                            relativeValue = (int64_t)(value - oldValue);
                        }

                        // Handle sign-cases where we have to turn them negative
                        if (relativeValue > collectionItem->Stats.LogicalMax
                            && collectionItem->Stats.LogicalMin < 0) {
                            if (relativeValue & (int64_t)(1 << (length - 1))) {
                                relativeValue -= (int64_t)(1 << length);
                            }
                        }

                        if (relativeValue != 0) {
                            char* debugAxis;
                            if (usage == HID_REPORT_USAGE_X_AXIS) {
                                debugAxis = "X";
                                eventData.inputData.rel_x = (int16_t)(relativeValue & 0xFFFF);
                            }
                            else if (usage == HID_REPORT_USAGE_Y_AXIS) {
                                debugAxis = "Y";
                                eventData.inputData.rel_y = (int16_t)(relativeValue & 0xFFFF);
                            }
                            else { // HID_REPORT_USAGE_Z_AXIS
                                debugAxis = "Z";
                                eventData.inputData.rel_z = (int16_t)(relativeValue & 0xFFFF);
                            }

                            TRACE("%s-Change: %i (Original 0x%x, Old 0x%x, LogMax %i)",
                                  debugAxis, (int32_t)relativeValue, (uint32_t)value, (uint32_t)oldValue,
                                  collectionItem->Stats.LogicalMax);
                        }

                    } break;
                }
            } break;

            // Describes keyboard or keypad events
            case HID_REPORT_USAGE_PAGE_KEYBOARD: {

            } break;

            // Generic Button events
            case HID_REPORT_USAGE_PAGE_BUTTON: {
                uint8_t keystateChanged = 0;

                // Check against old values if any changes are neccessary
                if (value != oldValue) {
                    keystateChanged = 1;
                }
                else {
                    break;
                }

                // Ok, so if we have multiple buttons (an array) 
                // we will use the logical min & max to find out which
                // button id this is
                TRACE("Button %u: %u", i, (uint32_t)value);

                // Possible types are: Keyboard, keypad, mouse, gamepad or joystick
                switch (collectionItem->InputType) {
                    // Mouse button event
                    case DeviceInputPointer: {

                    } break;

                    // Gamepad button event
                    case DeviceInputGamePad: {

                    } break;

                    // Joystick button event
                    case DeviceInputJoystick: {

                    } break;

                    // Ignore the rest of the input-types
                    default:
                        break;
                }
            } break;

            // Consumer, this is device-specific
            case HID_REPORT_USAGE_PAGE_CONSUMER: {
                // Virtual box sends me 0x238 which means AC Pan 
                // which actually is a kind of scrolling 
                // From the HID Usage Table Specs:
                // Sel - Set the horizontal offset of the display in the document. 
            } break;

            // We don't handle rest of usage-pages, but should be ok
            default: {
                TRACE("Usage Page 0x%x (Input Type 0x%x), Usage 0x%x, Value 0x%x",
                      collectionItem->Stats.UsagePage, collectionItem->InputType, usage, (uint32_t)value);
            } break;
        }
    }
    
    // Create a new input report
    if (collectionItem->InputType == DeviceInputKeyboard) {
        // send something
        ctt_input_event_button_all(hidDevice->Base.Base.Id, 0, 0);
    }
    else if (collectionItem->InputType == DeviceInputPointer) {
        // send something else
        ctt_input_event_cursor_all(hidDevice->Base.Base.Id, 0, 0, 0, 0, 0);
    }
}

int
HidParseReport(
    _In_ HidDevice_t*              hidDevice,
    _In_ UsbHidReportCollection_t* reportCollection,
    _In_ size_t                    dataIndex)
{
    UsbHidReportCollectionItem_t* itr;
    int                           calls = 0;

    itr = reportCollection->Childs;
    while (itr != NULL) {
        switch (itr->CollectionType) {

            // Collections inside collections must be parsed
            // recursively, so handle them
            case HID_TYPE_COLLECTION: {
                // Sanitize data attached
                if (itr->ItemPointer == NULL) {
                    break;
                }
                
                // Recursive parser for sub-collections
                calls += HidParseReport(hidDevice,
                                        (UsbHidReportCollection_t*)itr->ItemPointer, dataIndex);
            } break;
            
            // Input reports are interesting, that means we have an input event
            case HID_TYPE_INPUT: {
                HidParseReportInput(hidDevice, itr, dataIndex);
                calls++;
            } break;

            // For now we don't handle feature-reports
            // output reports are not handled here, but never should
            default:
                break;
        }
        
        // Go to next collection item in the collection
        itr = itr->Link;
    }

    // Return the number of actual parses we made
    return calls;
}
