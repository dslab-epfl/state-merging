/*
*
 * This is a quick and dirty hack to avoid external calls to __ctype_b_loc by ctype.h functions isspace, isalpha ...
 * To get the file for your locale run:

#include <stdio.h>
#include <ctype.h>


void main() {
        int i;
        const unsigned short * locale = *__ctype_b_loc ();
        for (i = 0; i<256; i++)
                printf("\t\tlocaled[%d] = %d;\n", i, locale[i]);
        printf("\n");
}

 *
 * And don't mind the "warning: dereferencing type-punned pointer will break strict-aliasing rules", it's okay :)
 *
 */
#include <string.h>
#include <stdlib.h>

unsigned short *locale = NULL; 

const unsigned short **__ctype_b_loc() {
	if (locale == NULL) {
		locale = (unsigned short *) malloc(256 * sizeof(unsigned short));
		locale[0] = 2;
		locale[1] = 2;
		locale[2] = 2;
		locale[3] = 2;
		locale[4] = 2;
		locale[5] = 2;
		locale[6] = 2;
		locale[7] = 2;
		locale[8] = 2;
		locale[9] = 8195;
		locale[10] = 8194;
		locale[11] = 8194;
		locale[12] = 8194;
		locale[13] = 8194;
		locale[14] = 2;
		locale[15] = 2;
		locale[16] = 2;
		locale[17] = 2;
		locale[18] = 2;
		locale[19] = 2;
		locale[20] = 2;
		locale[21] = 2;
		locale[22] = 2;
		locale[23] = 2;
		locale[24] = 2;
		locale[25] = 2;
		locale[26] = 2;
		locale[27] = 2;
		locale[28] = 2;
		locale[29] = 2;
		locale[30] = 2;
		locale[31] = 2;
		locale[32] = 24577;
		locale[33] = 49156;
		locale[34] = 49156;
		locale[35] = 49156;
		locale[36] = 49156;
		locale[37] = 49156;
		locale[38] = 49156;
		locale[39] = 49156;
		locale[40] = 49156;
		locale[41] = 49156;
		locale[42] = 49156;
		locale[43] = 49156;
		locale[44] = 49156;
		locale[45] = 49156;
		locale[46] = 49156;
		locale[47] = 49156;
		locale[48] = 55304;
		locale[49] = 55304;
		locale[50] = 55304;
		locale[51] = 55304;
		locale[52] = 55304;
		locale[53] = 55304;
		locale[54] = 55304;
		locale[55] = 55304;
		locale[56] = 55304;
		locale[57] = 55304;
		locale[58] = 49156;
		locale[59] = 49156;
		locale[60] = 49156;
		locale[61] = 49156;
		locale[62] = 49156;
		locale[63] = 49156;
		locale[64] = 49156;
		locale[65] = 54536;
		locale[66] = 54536;
		locale[67] = 54536;
		locale[68] = 54536;
		locale[69] = 54536;
		locale[70] = 54536;
		locale[71] = 50440;
		locale[72] = 50440;
		locale[73] = 50440;
		locale[74] = 50440;
		locale[75] = 50440;
		locale[76] = 50440;
		locale[77] = 50440;
		locale[78] = 50440;
		locale[79] = 50440;
		locale[80] = 50440;
		locale[81] = 50440;
		locale[82] = 50440;
		locale[83] = 50440;
		locale[84] = 50440;
		locale[85] = 50440;
		locale[86] = 50440;
		locale[87] = 50440;
		locale[88] = 50440;
		locale[89] = 50440;
		locale[90] = 50440;
		locale[91] = 49156;
		locale[92] = 49156;
		locale[93] = 49156;
		locale[94] = 49156;
		locale[95] = 49156;
		locale[96] = 49156;
		locale[97] = 54792;
		locale[98] = 54792;
		locale[99] = 54792;
		locale[100] = 54792;
		locale[101] = 54792;
		locale[102] = 54792;
		locale[103] = 50696;
		locale[104] = 50696;
		locale[105] = 50696;
		locale[106] = 50696;
		locale[107] = 50696;
		locale[108] = 50696;
		locale[109] = 50696;
		locale[110] = 50696;
		locale[111] = 50696;
		locale[112] = 50696;
		locale[113] = 50696;
		locale[114] = 50696;
		locale[115] = 50696;
		locale[116] = 50696;
		locale[117] = 50696;
		locale[118] = 50696;
		locale[119] = 50696;
		locale[120] = 50696;
		locale[121] = 50696;
		locale[122] = 50696;
		locale[123] = 49156;
		locale[124] = 49156;
		locale[125] = 49156;
		locale[126] = 49156;
		locale[127] = 2;
		locale[128] = 0;
		locale[129] = 0;
		locale[130] = 0;
		locale[131] = 0;
		locale[132] = 0;
		locale[133] = 0;
		locale[134] = 0;
		locale[135] = 0;
		locale[136] = 0;
		locale[137] = 0;
		locale[138] = 0;
		locale[139] = 0;
		locale[140] = 0;
		locale[141] = 0;
		locale[142] = 0;
		locale[143] = 0;
		locale[144] = 0;
		locale[145] = 0;
		locale[146] = 0;
		locale[147] = 0;
		locale[148] = 0;
		locale[149] = 0;
		locale[150] = 0;
		locale[151] = 0;
		locale[152] = 0;
		locale[153] = 0;
		locale[154] = 0;
		locale[155] = 0;
		locale[156] = 0;
		locale[157] = 0;
		locale[158] = 0;
		locale[159] = 0;
		locale[160] = 0;
		locale[161] = 0;
		locale[162] = 0;
		locale[163] = 0;
		locale[164] = 0;
		locale[165] = 0;
		locale[166] = 0;
		locale[167] = 0;
		locale[168] = 0;
		locale[169] = 0;
		locale[170] = 0;
		locale[171] = 0;
		locale[172] = 0;
		locale[173] = 0;
		locale[174] = 0;
		locale[175] = 0;
		locale[176] = 0;
		locale[177] = 0;
		locale[178] = 0;
		locale[179] = 0;
		locale[180] = 0;
		locale[181] = 0;
		locale[182] = 0;
		locale[183] = 0;
		locale[184] = 0;
		locale[185] = 0;
		locale[186] = 0;
		locale[187] = 0;
		locale[188] = 0;
		locale[189] = 0;
		locale[190] = 0;
		locale[191] = 0;
		locale[192] = 0;
		locale[193] = 0;
		locale[194] = 0;
		locale[195] = 0;
		locale[196] = 0;
		locale[197] = 0;
		locale[198] = 0;
		locale[199] = 0;
		locale[200] = 0;
		locale[201] = 0;
		locale[202] = 0;
		locale[203] = 0;
		locale[204] = 0;
		locale[205] = 0;
		locale[206] = 0;
		locale[207] = 0;
		locale[208] = 0;
		locale[209] = 0;
		locale[210] = 0;
		locale[211] = 0;
		locale[212] = 0;
		locale[213] = 0;
		locale[214] = 0;
		locale[215] = 0;
		locale[216] = 0;
		locale[217] = 0;
		locale[218] = 0;
		locale[219] = 0;
		locale[220] = 0;
		locale[221] = 0;
		locale[222] = 0;
		locale[223] = 0;
		locale[224] = 0;
		locale[225] = 0;
		locale[226] = 0;
		locale[227] = 0;
		locale[228] = 0;
		locale[229] = 0;
		locale[230] = 0;
		locale[231] = 0;
		locale[232] = 0;
		locale[233] = 0;
		locale[234] = 0;
		locale[235] = 0;
		locale[236] = 0;
		locale[237] = 0;
		locale[238] = 0;
		locale[239] = 0;
		locale[240] = 0;
		locale[241] = 0;
		locale[242] = 0;
		locale[243] = 0;
		locale[244] = 0;
		locale[245] = 0;
		locale[246] = 0;
		locale[247] = 0;
		locale[248] = 0;
		locale[249] = 0;
		locale[250] = 0;
		locale[251] = 0;
		locale[252] = 0;
		locale[253] = 0;
		locale[254] = 0;
		locale[255] = 0;
	}	
	return (const unsigned short **) &locale;
}
