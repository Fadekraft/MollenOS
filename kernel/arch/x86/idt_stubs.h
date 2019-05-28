/* MollenOS
 *
 * Copyright 2019, Philip Meulengracht
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
 * X86 IDT Assembler stubs
 *
 */

#ifndef __X86_IDT_STUBS_H__
#define __X86_IDT_STUBS_H__

#include <os/osdefs.h>

#define IDT_DESCRIPTOR_COUNT 256

typedef void(*idt_stub_t)(void);

__EXTERN void irq_handler0(void);
__EXTERN void irq_handler1(void);
__EXTERN void irq_handler2(void);
__EXTERN void irq_handler3(void);
__EXTERN void irq_handler4(void);
__EXTERN void irq_handler5(void);
__EXTERN void irq_handler6(void);
__EXTERN void irq_handler7(void);
__EXTERN void irq_handler8(void);
__EXTERN void irq_handler9(void);
__EXTERN void irq_handler10(void);
__EXTERN void irq_handler11(void);
__EXTERN void irq_handler12(void);
__EXTERN void irq_handler13(void);
__EXTERN void irq_handler14(void);
__EXTERN void irq_handler15(void);
__EXTERN void irq_handler16(void);
__EXTERN void irq_handler17(void);
__EXTERN void irq_handler18(void);
__EXTERN void irq_handler19(void);
__EXTERN void irq_handler20(void);
__EXTERN void irq_handler21(void);
__EXTERN void irq_handler22(void);
__EXTERN void irq_handler23(void);
__EXTERN void irq_handler24(void);
__EXTERN void irq_handler25(void);
__EXTERN void irq_handler26(void);
__EXTERN void irq_handler27(void);
__EXTERN void irq_handler28(void);
__EXTERN void irq_handler29(void);
__EXTERN void irq_handler30(void);
__EXTERN void irq_handler31(void);
__EXTERN void irq_handler32(void); 
__EXTERN void irq_handler33(void);
__EXTERN void irq_handler34(void);
__EXTERN void irq_handler35(void);
__EXTERN void irq_handler36(void);
__EXTERN void irq_handler37(void);
__EXTERN void irq_handler38(void);
__EXTERN void irq_handler39(void);
__EXTERN void irq_handler40(void);
__EXTERN void irq_handler41(void);
__EXTERN void irq_handler42(void);
__EXTERN void irq_handler43(void);
__EXTERN void irq_handler44(void);
__EXTERN void irq_handler45(void);
__EXTERN void irq_handler46(void);
__EXTERN void irq_handler47(void);
__EXTERN void irq_handler48(void);
__EXTERN void irq_handler49(void);
__EXTERN void irq_handler50(void);
__EXTERN void irq_handler51(void);
__EXTERN void irq_handler52(void);
__EXTERN void irq_handler53(void);
__EXTERN void irq_handler54(void);
__EXTERN void irq_handler55(void);
__EXTERN void irq_handler56(void);
__EXTERN void irq_handler57(void);
__EXTERN void irq_handler58(void);
__EXTERN void irq_handler59(void);
__EXTERN void irq_handler60(void);
__EXTERN void irq_handler61(void);
__EXTERN void irq_handler62(void);
__EXTERN void irq_handler63(void);
__EXTERN void irq_handler64(void);
__EXTERN void irq_handler65(void);
__EXTERN void irq_handler66(void);
__EXTERN void irq_handler67(void);
__EXTERN void irq_handler68(void);
__EXTERN void irq_handler69(void);
__EXTERN void irq_handler70(void);
__EXTERN void irq_handler71(void);
__EXTERN void irq_handler72(void);
__EXTERN void irq_handler73(void);
__EXTERN void irq_handler74(void);
__EXTERN void irq_handler75(void);
__EXTERN void irq_handler76(void);
__EXTERN void irq_handler77(void);
__EXTERN void irq_handler78(void);
__EXTERN void irq_handler79(void);
__EXTERN void irq_handler80(void);
__EXTERN void irq_handler81(void);
__EXTERN void irq_handler82(void);
__EXTERN void irq_handler83(void);
__EXTERN void irq_handler84(void);
__EXTERN void irq_handler85(void);
__EXTERN void irq_handler86(void);
__EXTERN void irq_handler87(void);
__EXTERN void irq_handler88(void);
__EXTERN void irq_handler89(void);
__EXTERN void irq_handler90(void);
__EXTERN void irq_handler91(void);
__EXTERN void irq_handler92(void);
__EXTERN void irq_handler93(void);
__EXTERN void irq_handler94(void);
__EXTERN void irq_handler95(void);
__EXTERN void irq_handler96(void);
__EXTERN void irq_handler97(void);
__EXTERN void irq_handler98(void);
__EXTERN void irq_handler99(void);
__EXTERN void irq_handler100(void);
__EXTERN void irq_handler101(void);
__EXTERN void irq_handler102(void);
__EXTERN void irq_handler103(void);
__EXTERN void irq_handler104(void);
__EXTERN void irq_handler105(void);
__EXTERN void irq_handler106(void);
__EXTERN void irq_handler107(void);
__EXTERN void irq_handler108(void);
__EXTERN void irq_handler109(void);
__EXTERN void irq_handler110(void);
__EXTERN void irq_handler111(void);
__EXTERN void irq_handler112(void);
__EXTERN void irq_handler113(void);
__EXTERN void irq_handler114(void);
__EXTERN void irq_handler115(void);
__EXTERN void irq_handler116(void);
__EXTERN void irq_handler117(void);
__EXTERN void irq_handler118(void);
__EXTERN void irq_handler119(void);
__EXTERN void irq_handler120(void);
__EXTERN void irq_handler121(void);
__EXTERN void irq_handler122(void);
__EXTERN void irq_handler123(void);
__EXTERN void irq_handler124(void);
__EXTERN void irq_handler125(void);
__EXTERN void irq_handler126(void);
__EXTERN void irq_handler127(void);
__EXTERN void irq_handler128(void);
__EXTERN void irq_handler129(void);
__EXTERN void irq_handler130(void);
__EXTERN void irq_handler131(void);
__EXTERN void irq_handler132(void);
__EXTERN void irq_handler133(void);
__EXTERN void irq_handler134(void);
__EXTERN void irq_handler135(void);
__EXTERN void irq_handler136(void);
__EXTERN void irq_handler137(void);
__EXTERN void irq_handler138(void);
__EXTERN void irq_handler139(void);
__EXTERN void irq_handler140(void);
__EXTERN void irq_handler141(void);
__EXTERN void irq_handler142(void);
__EXTERN void irq_handler143(void);
__EXTERN void irq_handler144(void);
__EXTERN void irq_handler145(void);
__EXTERN void irq_handler146(void);
__EXTERN void irq_handler147(void);
__EXTERN void irq_handler148(void);
__EXTERN void irq_handler149(void);
__EXTERN void irq_handler150(void);
__EXTERN void irq_handler151(void);
__EXTERN void irq_handler152(void);
__EXTERN void irq_handler153(void);
__EXTERN void irq_handler154(void);
__EXTERN void irq_handler155(void);
__EXTERN void irq_handler156(void);
__EXTERN void irq_handler157(void);
__EXTERN void irq_handler158(void);
__EXTERN void irq_handler159(void);
__EXTERN void irq_handler160(void);
__EXTERN void irq_handler161(void);
__EXTERN void irq_handler162(void);
__EXTERN void irq_handler163(void);
__EXTERN void irq_handler164(void);
__EXTERN void irq_handler165(void);
__EXTERN void irq_handler166(void);
__EXTERN void irq_handler167(void);
__EXTERN void irq_handler168(void);
__EXTERN void irq_handler169(void);
__EXTERN void irq_handler170(void);
__EXTERN void irq_handler171(void);
__EXTERN void irq_handler172(void);
__EXTERN void irq_handler173(void);
__EXTERN void irq_handler174(void);
__EXTERN void irq_handler175(void);
__EXTERN void irq_handler176(void);
__EXTERN void irq_handler177(void);
__EXTERN void irq_handler178(void);
__EXTERN void irq_handler179(void);
__EXTERN void irq_handler180(void);
__EXTERN void irq_handler181(void);
__EXTERN void irq_handler182(void);
__EXTERN void irq_handler183(void);
__EXTERN void irq_handler184(void);
__EXTERN void irq_handler185(void);
__EXTERN void irq_handler186(void);
__EXTERN void irq_handler187(void);
__EXTERN void irq_handler188(void);
__EXTERN void irq_handler189(void);
__EXTERN void irq_handler190(void);
__EXTERN void irq_handler191(void);
__EXTERN void irq_handler192(void);
__EXTERN void irq_handler193(void);
__EXTERN void irq_handler194(void);
__EXTERN void irq_handler195(void);
__EXTERN void irq_handler196(void);
__EXTERN void irq_handler197(void);
__EXTERN void irq_handler198(void);
__EXTERN void irq_handler199(void);
__EXTERN void irq_handler200(void);
__EXTERN void irq_handler201(void);
__EXTERN void irq_handler202(void);
__EXTERN void irq_handler203(void);
__EXTERN void irq_handler204(void);
__EXTERN void irq_handler205(void);
__EXTERN void irq_handler206(void);
__EXTERN void irq_handler207(void);
__EXTERN void irq_handler208(void);
__EXTERN void irq_handler209(void);
__EXTERN void irq_handler210(void);
__EXTERN void irq_handler211(void);
__EXTERN void irq_handler212(void);
__EXTERN void irq_handler213(void);
__EXTERN void irq_handler214(void);
__EXTERN void irq_handler215(void);
__EXTERN void irq_handler216(void);
__EXTERN void irq_handler217(void);
__EXTERN void irq_handler218(void);
__EXTERN void irq_handler219(void);
__EXTERN void irq_handler220(void);
__EXTERN void irq_handler221(void);
__EXTERN void irq_handler222(void);
__EXTERN void irq_handler223(void);
__EXTERN void irq_handler224(void);
__EXTERN void irq_handler225(void);
__EXTERN void irq_handler226(void);
__EXTERN void irq_handler227(void);
__EXTERN void irq_handler228(void);
__EXTERN void irq_handler229(void);
__EXTERN void irq_handler230(void);
__EXTERN void irq_handler231(void);
__EXTERN void irq_handler232(void);
__EXTERN void irq_handler233(void);
__EXTERN void irq_handler234(void);
__EXTERN void irq_handler235(void);
__EXTERN void irq_handler236(void);
__EXTERN void irq_handler237(void);
__EXTERN void irq_handler238(void);
__EXTERN void irq_handler239(void);
__EXTERN void irq_handler240(void);
__EXTERN void irq_handler241(void);
__EXTERN void irq_handler242(void);
__EXTERN void irq_handler243(void);
__EXTERN void irq_handler244(void);
__EXTERN void irq_handler245(void);
__EXTERN void irq_handler246(void);
__EXTERN void irq_handler247(void);
__EXTERN void irq_handler248(void);
__EXTERN void irq_handler249(void);
__EXTERN void irq_handler250(void);
__EXTERN void irq_handler251(void);
__EXTERN void irq_handler252(void);
__EXTERN void irq_handler253(void);
__EXTERN void irq_handler254(void);
__EXTERN void irq_handler255(void);

#endif //!__X86_IDT_STUBS_H__
