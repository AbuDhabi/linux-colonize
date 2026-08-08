typedef unsigned char   undefined;

typedef unsigned int    uint;
typedef unsigned char    undefined1;
typedef unsigned int    undefined2;
typedef unsigned int    word;
typedef struct OLD_IMAGE_DOS_RELOC OLD_IMAGE_DOS_RELOC, *POLD_IMAGE_DOS_RELOC;

struct OLD_IMAGE_DOS_RELOC {
    word offset;
    word segment;
};

typedef struct OLD_IMAGE_DOS_HEADER OLD_IMAGE_DOS_HEADER, *POLD_IMAGE_DOS_HEADER;

struct OLD_IMAGE_DOS_HEADER {
    char e_magic[2]; // Magic number
    word e_cblp; // Bytes of last page
    word e_cp; // Pages in file
    word e_crlc; // Relocations
    word e_cparhdr; // Size of header in paragraphs
    word e_minalloc; // Minimum extra paragraphs needed
    word e_maxalloc; // Maximum extra paragraphs needed
    word e_ss; // Initial (relative) SS value
    word e_sp; // Initial SP value
    word e_csum; // Checksum
    word e_ip; // Initial IP value
    word e_cs; // Initial (relative) CS value
    word e_lfarlc; // File address of relocation table
    word e_ovno; // Overlay number
};



int DAT_1322_825f;
undefined DAT_1000_e783;
undefined DAT_1000_e781;
undefined DAT_1000_e77f;
byte DAT_1322_0099;
byte DAT_1322_009a;
byte DAT_1322_009b;
uint DAT_1322_00f8;
uint DAT_1322_00fa;
uint DAT_1322_00fe;
uint DAT_1322_00fc;
undefined1 DAT_1000_1b39;
uint DAT_1000_1b24;
undefined2 DAT_1000_1b26;
undefined1 DAT_1000_1b28;
undefined1 DAT_1000_1b29;
uint DAT_1000_1b2a;
uint DAT_1000_1b2c;
uint DAT_1000_1b2e;
undefined2 DAT_1000_1b30;
undefined2 DAT_1000_1b32;
undefined2 DAT_1000_1b34;
undefined2 DAT_1000_2318;
undefined2 DAT_1000_231a;
undefined2 DAT_1000_231c;
undefined2 DAT_1000_231e;
undefined2 DAT_1000_1b6f;
undefined2 DAT_1000_1c73;
undefined2 DAT_1000_1c75;
undefined1 DAT_1000_1b38;
undefined2 *DAT_1000_1c73;
uint DAT_1000_1b6f;
char DAT_1000_1b3a;
undefined2 DAT_1000_288a;
undefined2 UNK_0000_019e;
int DAT_1000_10f0;
undefined2 DAT_1322_00e0;
uint DAT_1322_00cf;
int DAT_1322_00d1;
uint DAT_1000_288e;
int DAT_1000_2890;
uint DAT_1000_2892;
int DAT_1000_2894;
undefined2 DAT_1322_00e4;
undefined2 DAT_1000_288c;
int DAT_1322_00e2;

// WARNING: This function may have set the stack pointer
// WARNING: Globals starting with '_' overlap smaller symbols at the same address

void __cdecl16near entry(void)

{
  byte *pbVar1;
  byte bVar2;
  undefined1 uVar3;
  char cVar4;
  byte bVar5;
  int in_AX;
  int in_CX;
  byte bVar6;
  byte bVar7;
  undefined2 in_DX;
  byte bVar8;
  byte bVar9;
  int in_BX;
  int iVar10;
  byte bVar11;
  int unaff_BP;
  undefined2 *unaff_SI;
  undefined1 *unaff_DI;
  int iVar12;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  bool bVar13;
  
  iVar10 = in_BX + 1;
  out(*unaff_SI,in_DX);
  uVar3 = in(in_DX);
  *unaff_DI = uVar3;
  out(unaff_SI[1],in_DX);
  out(*(undefined1 *)(unaff_SI + 2),in_DX);
  iVar12 = *(int *)((int)unaff_SI + unaff_BP + 0x25) * 0x4d47;
  bVar11 = (byte)((uint)iVar10 >> 8);
  *(byte *)(iVar10 + iVar12) = *(byte *)(iVar10 + iVar12) ^ bVar11;
  DAT_1322_825f = in_AX + -0x666a;
  _DAT_1000_e783 = (byte *)(unaff_SI + 2);
  _DAT_1000_e783[iVar10] = _DAT_1000_e783[iVar10] + (char)DAT_1322_825f;
  _DAT_1000_e783[iVar10] = _DAT_1000_e783[iVar10] + (byte)in_DX;
  bVar8 = (byte)((uint)in_DX >> 8);
  bVar7 = (byte)in_DX & *(byte *)(unaff_BP + iVar12 + -1);
  *_DAT_1000_e783 = *_DAT_1000_e783 & 0xb;
  pbVar1 = _DAT_1000_e783 + in_BX + 0x1a;
  bVar2 = *pbVar1;
  bVar9 = (byte)iVar10;
  *pbVar1 = *pbVar1 + bVar9;
  bVar5 = bVar7 - *(byte *)(in_BX + -0x45e5);
  pbVar1 = (byte *)(unaff_BP + iVar12 + -1 + -0x4ae6);
  bVar6 = (byte)((uint)(in_CX + -1) >> 8);
  bVar13 = bVar6 < *pbVar1 ||
           (byte)(bVar6 - *pbVar1) <
           (bVar7 < *(byte *)(in_BX + -0x45e5) || bVar5 < CARRY1(bVar2,bVar9));
  bVar6 = bVar8 - *(byte *)(unaff_BP + -0x48e6);
  in(CONCAT11(bVar6 - bVar13,
              ((bVar5 - CARRY1(bVar2,bVar9)) - *(char *)(iVar12 + -0x76)) -
              (bVar11 < _DAT_1000_e783[in_BX + -0x46e5] ||
              (byte)(bVar11 - _DAT_1000_e783[in_BX + -0x46e5]) <
              (bVar8 < *(byte *)(unaff_BP + -0x48e6) || bVar6 < bVar13))));
  *(undefined1 *)(unaff_BP + -4) = 0;
  do {
    cVar4 = *(char *)((uint)*(byte *)0x81fe * 4 + (*(uint *)(unaff_BP + -4) & 0xff) + -0x7e00);
    *(char *)(unaff_BP + -2) = cVar4;
    if (cVar4 == -1) {
      return;
    }
    *(byte *)0x59 = *(byte *)0x81fe | 0x90;
    if (*(char *)0x59 != *(char *)0x58) {
      _DAT_1000_e781 = (uint)*(byte *)0x59;
      _DAT_1000_e77f = 0x89;
      FUN_1000_1395();
      *(undefined1 *)0x58 = *(undefined1 *)0x59;
    }
    _DAT_1000_e781 = (uint)*(byte *)(unaff_BP + -2);
    _DAT_1000_e77f = 0x9b;
    FUN_1000_1395();
    _DAT_1000_e781 = 0;
    _DAT_1000_e77f = 0xa4;
    FUN_1000_1395();
    *(undefined1 *)((uint)*(byte *)0x81fe * 4 + (*(uint *)(unaff_BP + -4) & 0xff) + -0x7e00) = 0xff;
    *(char *)(unaff_BP + -4) = *(char *)(unaff_BP + -4) + '\x01';
  } while (*(byte *)(unaff_BP + -4) < 4);
  return;
}



void __cdecl16near FUN_1000_00cb(undefined1 param_1)

{
  undefined2 unaff_DS;
  
  *(byte *)0x59 = *(byte *)0x81fe | 0x90;
  if (*(char *)0x59 != *(char *)0x58) {
    FUN_1000_1395(*(undefined1 *)0x59);
    *(undefined1 *)0x58 = *(undefined1 *)0x59;
  }
  FUN_1000_1395(param_1);
  FUN_1000_1395(*(undefined1 *)(*(int *)0x8240 + 6));
  return;
}



void __cdecl16near FUN_1000_010f(void)

{
  undefined2 unaff_DS;
  
  *(byte *)0x59 = *(byte *)0x81fe | 0xc0;
  if (*(char *)0x59 != *(char *)0x58) {
    FUN_1000_1395(*(undefined1 *)0x59);
    *(undefined1 *)0x58 = *(undefined1 *)0x59;
  }
  FUN_1000_1395(*(undefined1 *)(*(int *)0x8240 + 5));
  return;
}



void __cdecl16near FUN_1000_0143(undefined1 param_1,undefined1 param_2)

{
  undefined2 unaff_DS;
  
  *(byte *)0x59 = *(byte *)0x81fe | 0xb0;
  if (*(char *)0x59 != *(char *)0x58) {
    FUN_1000_1395(*(undefined1 *)0x59);
    *(undefined1 *)0x58 = *(undefined1 *)0x59;
  }
  FUN_1000_1395(param_1);
  FUN_1000_1395(param_2);
  return;
}



void __cdecl16near FUN_1000_0183(void)

{
  undefined2 unaff_DS;
  
  *(byte *)0x59 = *(byte *)0x81fe | 0xe0;
  if (*(char *)0x59 != *(char *)0x58) {
    FUN_1000_1395(*(undefined1 *)0x59);
    *(undefined1 *)0x58 = *(undefined1 *)0x59;
  }
  FUN_1000_1395(0);
  FUN_1000_1395((int)*(char *)(*(int *)0x8240 + 0x11));
  return;
}



void __cdecl16near FUN_1000_01bf(undefined2 param_1)

{
  undefined2 unaff_DS;
  
  if (*(int *)0x7c == 0) {
    FUN_1000_27b4(param_1);
  }
  return;
}



undefined2 __cdecl16near FUN_1000_01d6(void)

{
  undefined1 uVar1;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  *(int *)0x8094 = *(int *)0x8094 + 1;
  uVar1 = *(undefined1 *)*(undefined2 *)0x8094;
  *(int *)0x8094 = *(int *)0x8094 + 1;
  local_4 = CONCAT11(*(undefined1 *)*(undefined2 *)0x8094,uVar1);
  return local_4;
}



void __cdecl16near FUN_1000_01fd(void)

{
  int *piVar1;
  char *pcVar2;
  char *pcVar3;
  byte *pbVar4;
  undefined1 *puVar5;
  undefined1 *puVar6;
  char cVar7;
  byte bVar8;
  char cVar9;
  undefined2 uVar10;
  int iVar11;
  uint uVar12;
  uint uVar13;
  int iVar14;
  code *pcVar15;
  undefined2 unaff_DS;
  bool bVar16;
  bool bVar17;
  uint local_10;
  
  pcVar3 = (char *)*(undefined2 *)0x8240;
  if (*pcVar3 != '\0') {
    if (pcVar3[9] != '\0') {
      pcVar2 = pcVar3 + 9;
      *pcVar2 = *pcVar2 + -1;
      if (*pcVar2 == '\0') {
        func_0x00010048();
      }
    }
    pcVar2 = (char *)*(undefined2 *)0x8240;
    cVar7 = *pcVar2;
    *pcVar2 = *pcVar2 + -1;
    if (cVar7 == '\0') {
      do {
        pbVar4 = (byte *)*(undefined2 *)(*(int *)0x8240 + 0x16);
        *(undefined2 *)0x8094 = pbVar4;
        if (((*pbVar4 & 0x80) == 0) || ((char)*pbVar4 < -0x45)) break;
        switch(*(undefined1 *)*(undefined2 *)0x8094) {
        case 0xbb:
          FUN_1000_0143(0x65,0);
          FUN_1000_0143(100,0);
          *(int *)0x8094 = *(int *)0x8094 + 1;
          FUN_1000_0143(6,(int)*(char *)*(undefined2 *)0x8094);
          iVar11 = 0;
          uVar10 = 0x26;
          goto LAB_1000_0566;
        case 0xbc:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(int *)0x50 = (int)*(char *)*(undefined2 *)0x8094;
          *(undefined1 *)0x8092 = 0;
          *(undefined1 *)0x52 = 0;
          goto LAB_1000_044f;
        case 0xbd:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(char *)0x52 = *(char *)*(undefined2 *)0x8094 + -1;
          *(int *)(*(int *)0x8240 + 0x16) = *(int *)(*(int *)0x8240 + 0x16) + 2;
          *(undefined1 *)0x8092 = 0;
          break;
        case 0xbe:
          cVar7 = *(char *)0x54;
          cVar9 = *(char *)0x8091;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)0x54 = *(undefined1 *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)0x8091 = *(undefined1 *)*(undefined2 *)0x8094;
          if ((cVar7 != *(char *)0x54) || (cVar9 != *(char *)0x8091)) {
            cVar7 = *(char *)0x8091;
            cVar9 = *(char *)0x8090;
            *(char *)0x53 = cVar7 * cVar9;
            *(char *)0x8092 = cVar7 * cVar9;
            *(undefined1 *)0x52 = 1;
            *(int *)0x50 = *(int *)0x50 + 1;
          }
          goto LAB_1000_052a;
        case 0xbf:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)0x8090 = *(undefined1 *)*(undefined2 *)0x8094;
          cVar7 = *(char *)0x8091;
          cVar9 = *(char *)0x8090;
          *(char *)0x53 = cVar7 * cVar9;
          *(char *)0x8092 = cVar7 * cVar9;
          goto LAB_1000_044f;
        case 0xc0:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          iVar11 = (int)*(char *)*(undefined2 *)0x8094;
          uVar10 = 0;
          goto LAB_1000_0566;
        case 0xc1:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          iVar11 = (int)*(char *)*(undefined2 *)0x8094;
          uVar10 = 0x5d;
          goto LAB_1000_0566;
        case 0xc2:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          iVar11 = (int)*(char *)*(undefined2 *)0x8094;
          uVar10 = 0x5b;
          goto LAB_1000_0566;
        case 0xc3:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          FUN_1000_01bf((int)*(char *)*(undefined2 *)0x8094);
          goto LAB_1000_044f;
        case 0xc4:
          pcVar15 = (code *)FUN_1000_01d6();
          (*pcVar15)();
          goto LAB_1000_052a;
        case 0xc5:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(byte *)(cVar7 + 0x5c) < *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          bVar17 = *(byte *)(cVar7 + 0x5c) == *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0d00;
        case 0xc6:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(byte *)(cVar7 + 0x5c) < *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0cd1;
        case 199:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(char *)(cVar7 + 0x5c) == *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0ca2;
        case 200:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(char *)(cVar7 + 0x5c) == *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0c63;
        case 0xc9:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) < (uint)(int)*(char *)*(undefined2 *)0x8094;
          bVar17 = (uint)*(byte *)(cVar7 + 0x5c) == (int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0d00:
          if (bVar16 || bVar17) {
LAB_1000_0af8:
            *(int *)(*(int *)0x8240 + 0x16) = *(int *)(*(int *)0x8240 + 0x16) + 5;
          }
          else {
LAB_1000_0c68:
            *(int *)(*(int *)0x8240 + 0x22) = *(int *)(*(int *)0x8240 + 0x16) + 5;
LAB_1000_0aee:
            local_10 = FUN_1000_01d6();
            iVar11 = *(int *)0x8240;
LAB_1000_02f1:
            *(undefined2 *)(iVar11 + 0x16) = local_10;
          }
          break;
        case 0xca:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) < (uint)(int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0cd1:
          if (bVar16) goto LAB_1000_0c68;
          goto LAB_1000_0af8;
        case 0xcb:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) == (int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0ca2:
          if (!bVar16) goto LAB_1000_0c68;
          goto LAB_1000_0af8;
        case 0xcc:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) == (int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0c63:
          if (bVar16) goto LAB_1000_0c68;
          goto LAB_1000_0af8;
        case 0xcd:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(byte *)(cVar7 + 0x5c) < *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          bVar17 = *(byte *)(cVar7 + 0x5c) == *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0b83;
        case 0xce:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(byte *)(cVar7 + 0x5c) < *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0b57;
        case 0xcf:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(char *)(cVar7 + 0x5c) == *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0b2b;
        case 0xd0:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = *(char *)(cVar7 + 0x5c) == *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0aec;
        case 0xd1:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) < (uint)(int)*(char *)*(undefined2 *)0x8094;
          bVar17 = (uint)*(byte *)(cVar7 + 0x5c) == (int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0b83:
          if (!bVar16 && !bVar17) goto LAB_1000_0aee;
          goto LAB_1000_0af8;
        case 0xd2:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) < (uint)(int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0b57:
          if (bVar16) goto LAB_1000_0aee;
          goto LAB_1000_0af8;
        case 0xd3:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) == (int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0b2b:
          if (!bVar16) goto LAB_1000_0aee;
          goto LAB_1000_0af8;
        case 0xd4:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar16 = (uint)*(byte *)(cVar7 + 0x5c) == (int)*(char *)*(undefined2 *)0x8094;
LAB_1000_0aec:
          if (bVar16) goto LAB_1000_0aee;
          goto LAB_1000_0af8;
        case 0xd5:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0a96;
        case 0xd6:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)*(undefined2 *)0x8094;
LAB_1000_0a96:
          local_10 = (uint)cVar7;
          *(byte *)(local_10 + 0x5c) = *(byte *)(local_10 + 0x5c) ^ bVar8;
          goto LAB_1000_052a;
        case 0xd7:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_0a46;
        case 0xd8:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)*(undefined2 *)0x8094;
LAB_1000_0a46:
          local_10 = (uint)cVar7;
          *(byte *)(local_10 + 0x5c) = *(byte *)(local_10 + 0x5c) | bVar8;
          goto LAB_1000_052a;
        case 0xd9:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_09f6;
        case 0xda:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)*(undefined2 *)0x8094;
LAB_1000_09f6:
          local_10 = (uint)cVar7;
          *(byte *)(local_10 + 0x5c) = *(byte *)(local_10 + 0x5c) & bVar8;
          goto LAB_1000_052a;
        case 0xdb:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(byte *)(cVar7 + 0x5c) =
               *(byte *)(cVar7 + 0x5c) % *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_052a;
        case 0xdc:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(cVar7 + 0x5c) =
               (char)((uint)*(byte *)(cVar7 + 0x5c) % (uint)(int)*(char *)*(undefined2 *)0x8094);
          goto LAB_1000_052a;
        case 0xdd:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          local_10 = (uint)*(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)(local_10 + 0x5c) / *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_079d;
        case 0xde:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          local_10 = (uint)*(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = (byte)((uint)*(byte *)(local_10 + 0x5c) /
                        (uint)(int)*(char *)*(undefined2 *)0x8094);
          goto LAB_1000_079d;
        case 0xdf:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_08e8;
        case 0xe0:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
LAB_1000_08e8:
          local_10 = (uint)cVar9;
          bVar8 = cVar7 * *(char *)(local_10 + 0x5c);
          goto LAB_1000_079d;
        case 0xe1:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_089b;
        case 0xe2:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
LAB_1000_089b:
          local_10 = (uint)cVar9;
          *(char *)(local_10 + 0x5c) = *(char *)(local_10 + 0x5c) - cVar7;
          goto LAB_1000_052a;
        case 0xe3:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_084b;
        case 0xe4:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
LAB_1000_084b:
          local_10 = (uint)cVar9;
          *(char *)(local_10 + 0x5c) = *(char *)(local_10 + 0x5c) + cVar7;
          goto LAB_1000_052a;
        case 0xe5:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c) =
               *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c) + -1;
          goto LAB_1000_044f;
        case 0xe6:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c) =
               *(char *)(*(char *)*(undefined2 *)0x8094 + 0x5c) + '\x01';
          goto LAB_1000_044f;
        case 0xe7:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(*(int *)0x8094 + (int)*(char *)*(undefined2 *)0x8094 + 1) =
               *(undefined1 *)(cVar7 + 0x5c);
          goto LAB_1000_052a;
        case 0xe8:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          local_10 = (uint)*(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)(*(char *)*(undefined2 *)0x8094 + 0x5c);
          goto LAB_1000_079d;
        case 0xe9:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          local_10 = (uint)*(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          bVar8 = *(byte *)*(undefined2 *)0x8094;
LAB_1000_079d:
          *(byte *)(local_10 + 0x5c) = bVar8;
          goto LAB_1000_052a;
        case 0xea:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          iVar14 = (int)*(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(*(int *)0x8094 + *(char *)(iVar14 + *(int *)0x8094) + iVar14 + 1) =
               *(undefined1 *)((uint)*(byte *)(cVar7 + 0x5c) + *(int *)0x8094);
          iVar11 = *(int *)0x8240;
          iVar14 = iVar14 + 4;
          goto LAB_1000_06e4;
        case 0xeb:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar9 = *(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          uVar12 = FUN_1000_118a();
          local_10._0_1_ = (char)((uVar12 & 0x7fff) % (((int)cVar9 - (int)cVar7) + 1U));
          ((char *)*(undefined2 *)0x8094)[*(char *)*(undefined2 *)0x8094 + 1] =
               (char)local_10 + cVar7;
          goto LAB_1000_04a0;
        case 0xec:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          uVar12 = (uint)*(char *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          uVar13 = FUN_1000_118a();
          *(undefined1 *)(*(int *)0x8094 + (int)*(char *)(uVar12 + *(int *)0x8094) + uVar12 + 1) =
               *(undefined1 *)((uVar13 & 0x7fff) % uVar12 + *(int *)0x8094);
          iVar11 = *(int *)0x8240;
          iVar14 = uVar12 + 3;
LAB_1000_06e4:
          *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + iVar14;
          break;
        case 0xed:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          uVar12 = (uint)*(char *)*(undefined2 *)0x8094;
          if ((int)*(char *)(*(int *)0x8094 + 1) + (int)*(char *)(*(int *)0x8240 + 0x25) !=
              (uint)*(byte *)((uint)*(byte *)0x81fe * 4 + -0x7e00)) {
            func_0x00010048();
          }
          for (local_10 = 0; uVar13 = uVar12, local_10 < uVar12; local_10 = local_10 + 1) {
            cVar7 = *(char *)(*(int *)0x8240 + 0x25);
            *(int *)0x8094 = *(int *)0x8094 + 1;
            cVar7 = cVar7 + *(char *)*(undefined2 *)0x8094;
            if ((-1 < *(char *)(*(int *)0x8240 + 7)) ||
               (*(char *)(local_10 + (*(uint *)0x81fe & 0xff) * 4 + -0x7e00) != cVar7)) {
              *(char *)(local_10 + (*(uint *)0x81fe & 0xff) * 4 + -0x7e00) = cVar7;
              FUN_1000_00cb(cVar7);
            }
          }
          while (local_10 = uVar13, local_10 < 4) {
            *(undefined1 *)(local_10 + (*(uint *)0x81fe & 0xff) * 4 + -0x7e00) = 0xff;
            uVar13 = local_10 + 1;
          }
          pcVar3 = (char *)*(undefined2 *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *pcVar3 = *(char *)*(undefined2 *)0x8094;
          if (pcVar3[8] == '\0') {
            cVar7 = *pcVar3 - pcVar3[7];
          }
          else {
            cVar7 = pcVar3[8];
          }
          pcVar3[9] = cVar7;
          *(uint *)(pcVar3 + 0x16) = *(int *)(pcVar3 + 0x16) + uVar12 + 3;
          *(undefined1 *)0x5a = 1;
          break;
        case 0xee:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 0x25) = *(undefined1 *)*(undefined2 *)0x8094;
          goto LAB_1000_044f;
        case 0xef:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 0xe) = *(undefined1 *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 3) = *(undefined1 *)*(undefined2 *)0x8094;
          *(undefined1 *)(iVar11 + 0xd) = 1;
          goto LAB_1000_052a;
        case 0xf0:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          cVar7 = *(char *)*(undefined2 *)0x8094;
          *(char *)(iVar11 + 0xf) = cVar7;
          iVar11 = (int)cVar7;
          uVar10 = 10;
LAB_1000_0566:
          FUN_1000_0143(uVar10,iVar11);
          goto LAB_1000_044f;
        case 0xf1:
          *(int *)0x8094 = *(int *)0x8094 + 1;
          iVar11 = *(int *)0x8240;
          if ((*(char *)(iVar11 + 0x26) == '\0') ||
             ((uint)(int)*(char *)*(undefined2 *)0x8094 < (uint)*(byte *)(iVar11 + 0x10))) {
            *(char *)(iVar11 + 0x10) = *(char *)*(undefined2 *)0x8094;
          }
          iVar11 = *(int *)0x8240;
          piVar1 = (int *)(iVar11 + 0x16);
          *piVar1 = *piVar1 + 2;
          FUN_1000_0143(7,*(undefined1 *)(iVar11 + 0x10));
          break;
        case 0xf2:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 0x11) = *(undefined1 *)*(undefined2 *)0x8094;
          piVar1 = (int *)(iVar11 + 0x16);
          *piVar1 = *piVar1 + 2;
          FUN_1000_0183();
          break;
        case 0xf3:
          iVar11 = *(int *)0x8240;
          if (*(char *)(iVar11 + 0x26) == '\0') {
            *(int *)0x8094 = *(int *)0x8094 + 1;
            *(undefined1 *)(iVar11 + 0xb) = *(undefined1 *)*(undefined2 *)0x8094;
            *(int *)0x8094 = *(int *)0x8094 + 1;
            *(undefined1 *)(iVar11 + 2) = *(undefined1 *)*(undefined2 *)0x8094;
            *(undefined1 *)(iVar11 + 10) = 1;
          }
LAB_1000_052a:
          *(int *)(*(int *)0x8240 + 0x16) = *(int *)(*(int *)0x8240 + 0x16) + 3;
          break;
        case 0xf4:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 6) = *(undefined1 *)*(undefined2 *)0x8094;
          goto LAB_1000_044f;
        case 0xf5:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 0x12) = *(undefined1 *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 1) = *(undefined1 *)*(undefined2 *)0x8094;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 0x13) = *(undefined1 *)*(undefined2 *)0x8094;
          *(undefined1 *)(iVar11 + 0xc) = 1;
LAB_1000_04a0:
          *(int *)(*(int *)0x8240 + 0x16) = *(int *)(*(int *)0x8240 + 0x16) + 4;
          break;
        case 0xf6:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 8) = *(undefined1 *)*(undefined2 *)0x8094;
          *(undefined1 *)(iVar11 + 7) = 0;
          goto LAB_1000_044f;
        case 0xf7:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 7) = *(undefined1 *)*(undefined2 *)0x8094;
          *(undefined1 *)(iVar11 + 8) = 0;
LAB_1000_044f:
          *(int *)(*(int *)0x8240 + 0x16) = *(int *)(*(int *)0x8240 + 0x16) + 2;
          break;
        case 0xf8:
          iVar11 = *(int *)0x8240;
          *(int *)0x8094 = *(int *)0x8094 + 1;
          *(undefined1 *)(iVar11 + 5) = *(undefined1 *)*(undefined2 *)0x8094;
          piVar1 = (int *)(iVar11 + 0x16);
          *piVar1 = *piVar1 + 2;
          FUN_1000_010f();
          break;
        case 0xf9:
          iVar11 = *(int *)0x8240;
          if (*(int *)(iVar11 + 0x22) == 0) {
            *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 1;
          }
          else {
            *(undefined2 *)(iVar11 + 0x16) = *(undefined2 *)(iVar11 + 0x22);
            *(undefined2 *)(iVar11 + 0x22) = 0;
          }
          break;
        case 0xfa:
          local_10 = FUN_1000_01d6();
          *(int *)(*(int *)0x8240 + 0x22) = *(int *)(*(int *)0x8240 + 0x16) + 3;
          goto LAB_1000_03de;
        case 0xfb:
          local_10 = FUN_1000_01d6();
LAB_1000_03de:
          iVar11 = *(int *)0x8240;
          goto LAB_1000_02f1;
        case 0xfc:
          uVar10 = FUN_1000_01d6();
          iVar11 = *(int *)0x8240;
          *(undefined2 *)(iVar11 + 0x14) = uVar10;
          *(undefined2 *)(iVar11 + 0x16) = uVar10;
          *(undefined2 *)(iVar11 + 0x18) = uVar10;
          *(undefined2 *)(iVar11 + 0x1a) = uVar10;
          *(undefined2 *)(iVar11 + 0x20) = uVar10;
          break;
        case 0xfd:
          iVar11 = *(int *)0x8240;
          if (*(int *)(iVar11 + 0x20) == 0) {
            *(undefined2 *)(iVar11 + 0x16) = *(undefined2 *)(iVar11 + 0x14);
          }
          else {
            uVar10 = *(undefined2 *)(iVar11 + 0x20);
            *(undefined2 *)(iVar11 + 0x14) = uVar10;
            *(undefined2 *)(iVar11 + 0x16) = uVar10;
            *(undefined2 *)(iVar11 + 0x18) = uVar10;
            *(undefined2 *)(iVar11 + 0x1a) = uVar10;
          }
          if (*(char *)(iVar11 + 0x26) != -1) {
            *(undefined2 *)0x50 = 0;
          }
          break;
        case 0xfe:
          iVar11 = *(int *)0x8240;
          if (*(int *)(iVar11 + 0x1e) == 0) {
            *(int *)0x8094 = *(int *)0x8094 + 1;
            if (*(char *)*(undefined2 *)0x8094 == '\0') {
              iVar11 = *(int *)0x8240;
              *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 2;
              uVar10 = *(undefined2 *)(iVar11 + 0x16);
              *(undefined2 *)(iVar11 + 0x1a) = uVar10;
              *(undefined2 *)(iVar11 + 0x18) = uVar10;
              *(undefined2 *)(iVar11 + 0x1c) = 0;
              *(undefined2 *)(iVar11 + 0x1e) = 0;
              break;
            }
            iVar11 = *(int *)0x8240;
            *(int *)(iVar11 + 0x1e) = (int)*(char *)*(undefined2 *)0x8094;
            *(undefined2 *)(iVar11 + 0x16) = *(undefined2 *)(iVar11 + 0x1a);
          }
          else {
            piVar1 = (int *)(iVar11 + 0x1e);
            *piVar1 = *piVar1 + -1;
            if (*piVar1 == 0) {
              *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 2;
              uVar10 = *(undefined2 *)(iVar11 + 0x16);
              *(undefined2 *)(iVar11 + 0x1a) = uVar10;
              *(undefined2 *)(iVar11 + 0x18) = uVar10;
            }
            else {
              *(undefined2 *)(iVar11 + 0x16) = *(undefined2 *)(iVar11 + 0x1a);
            }
          }
LAB_1000_0306:
          *(undefined2 *)(iVar11 + 0x18) = *(undefined2 *)(iVar11 + 0x16);
          break;
        case 0xff:
          if (*(int *)(*(int *)0x8240 + 0x1c) == 0) {
            *(int *)0x8094 = *(int *)0x8094 + 1;
            if (*(char *)*(undefined2 *)0x8094 == '\0') {
              iVar11 = *(int *)0x8240;
              *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 2;
              *(undefined2 *)(iVar11 + 0x18) = *(undefined2 *)(iVar11 + 0x16);
              *(undefined2 *)(iVar11 + 0x1c) = 0;
              break;
            }
            iVar11 = *(int *)0x8240;
            *(int *)(iVar11 + 0x1c) = (int)*(char *)*(undefined2 *)0x8094;
          }
          else {
            iVar11 = *(int *)0x8240;
            piVar1 = (int *)(iVar11 + 0x1c);
            *piVar1 = *piVar1 + -1;
            if (*piVar1 == 0) {
              *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 2;
              goto LAB_1000_0306;
            }
            iVar11 = *(int *)0x8240;
          }
          local_10 = *(undefined2 *)(iVar11 + 0x18);
          goto LAB_1000_02f1;
        }
      } while (*(char *)0x5a == '\0');
      if (*(char *)0x5a == '\0') {
        pcVar3 = (char *)*(undefined2 *)(*(int *)0x8240 + 0x16);
        *(undefined2 *)0x8094 = pcVar3;
        *(int *)0x8094 = *(int *)0x8094 + 1;
        cVar7 = *pcVar3;
        puVar5 = (undefined1 *)*(undefined2 *)0x8240;
        puVar6 = (undefined1 *)*(undefined2 *)0x8094;
        *(int *)0x8094 = *(int *)0x8094 + 1;
        *puVar5 = *puVar6;
        *(int *)(puVar5 + 0x16) = *(int *)(puVar5 + 0x16) + 2;
        if (puVar5[4] != cVar7) {
          func_0x00010048();
        }
        if ((cVar7 == '\0') || (*(char *)*(undefined2 *)0x8240 == '\0')) {
          func_0x00010048();
        }
        else {
          pcVar3 = (char *)*(undefined2 *)0x8240;
          if (pcVar3[8] == '\0') {
            cVar9 = *pcVar3 - pcVar3[7];
          }
          else {
            cVar9 = pcVar3[8];
          }
          pcVar3[9] = cVar9;
          cVar7 = cVar7 + pcVar3[0x25];
          if ((-1 < pcVar3[7]) || (*(char *)((uint)*(byte *)0x81fe * 4 + -0x7e00) != cVar7)) {
            *(char *)(*(int *)0x8240 + 4) = cVar7 - *(char *)(*(int *)0x8240 + 0x25);
            *(char *)((uint)*(byte *)0x81fe * 4 + -0x7e00) = cVar7;
            FUN_1000_00cb(cVar7);
          }
        }
      }
      else {
        *(undefined1 *)0x5a = 0;
      }
    }
  }
  iVar11 = *(int *)0x8240;
  if (*(char *)(iVar11 + 2) != '\0') {
    pcVar2 = (char *)(iVar11 + 10);
    *pcVar2 = *pcVar2 + -1;
    if (*pcVar2 == '\0') {
      *(undefined1 *)(iVar11 + 10) = *(undefined1 *)(iVar11 + 0xb);
      *(char *)(iVar11 + 0x10) = *(char *)(iVar11 + 0x10) + *(char *)(iVar11 + 2);
      if (0x7f < *(byte *)(iVar11 + 0x10)) {
        *(undefined1 *)(iVar11 + 2) = 0;
        if (*(byte *)(iVar11 + 0x10) < 0xb0) {
          *(undefined1 *)(iVar11 + 0x10) = 0x7f;
        }
        else {
          *(undefined1 *)(iVar11 + 0x10) = 0;
        }
      }
      FUN_1000_0143(7,*(undefined1 *)(iVar11 + 0x10));
    }
  }
  iVar11 = *(int *)0x8240;
  if (*(char *)(iVar11 + 1) != '\0') {
    pcVar2 = (char *)(iVar11 + 0xc);
    *pcVar2 = *pcVar2 + -1;
    if (*pcVar2 == '\0') {
      *(undefined1 *)(iVar11 + 0xc) = *(undefined1 *)(iVar11 + 0x12);
      *(char *)(iVar11 + 0x11) = *(char *)(iVar11 + 0x11) + *(char *)(iVar11 + 1);
      FUN_1000_0183();
    }
    iVar11 = *(int *)0x8240;
    pcVar2 = (char *)(iVar11 + 0x13);
    *pcVar2 = *pcVar2 + -1;
    if (*pcVar2 == '\0') {
      *(undefined1 *)(iVar11 + 1) = 0;
    }
  }
  if (*(char *)(iVar11 + 3) != '\0') {
    pcVar2 = (char *)(iVar11 + 0xd);
    *pcVar2 = *pcVar2 + -1;
    if (*pcVar2 == '\0') {
      *(undefined1 *)(iVar11 + 0xd) = *(undefined1 *)(iVar11 + 0xe);
      *(char *)(iVar11 + 0xf) = *(char *)(iVar11 + 0xf) + *(char *)(iVar11 + 3);
      FUN_1000_0143(10,(int)*(char *)(iVar11 + 0xf));
    }
  }
  *(char *)0x81fe = *(char *)0x81fe + '\x01';
  return;
}



void __cdecl16near FUN_1000_118a(void)

{
  uint uVar1;
  undefined2 unaff_DS;
  
  uVar1 = *(int *)0x87 + 0x9249U >> 1;
  *(uint *)0x87 =
       ((uVar1 | (uint)((*(int *)0x87 + 0x9249U & 1) != 0) << 0xf) >> 1 |
       (uint)((uVar1 & 1) != 0) << 0xf) >> 1 | (uint)((uVar1 & 2) != 0) << 0xf;
  return;
}



undefined4 __cdecl16near FUN_1000_127a(void)

{
  undefined2 in_AX;
  int iVar1;
  undefined2 in_DX;
  undefined2 unaff_DS;
  
  iVar1 = 20000;
  do {
    in(*(undefined2 *)0x8c);
    iVar1 = iVar1 + -1;
  } while (iVar1 != 0);
  return CONCAT22(in_DX,in_AX);
}



undefined2 FUN_1000_12bc(void)

{
  byte bVar1;
  undefined2 uVar2;
  int iVar3;
  char in_BL;
  undefined2 unaff_DS;
  
  *(char *)0x80 = in_BL + *(char *)0x80;
  iVar3 = -1;
  do {
    bVar1 = in(*(undefined2 *)0x8c);
    if ((bVar1 & 0x40) == 0) {
      out(*(undefined2 *)0x8a,in_BL);
      uVar2 = 1;
      goto LAB_1000_12e7;
    }
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  uVar2 = 0;
LAB_1000_12e7:
  while (bVar1 = in(*(undefined2 *)0x8c), (bVar1 & 0x80) == 0) {
    in(*(undefined2 *)0x8a);
  }
  return uVar2;
}



undefined2 __cdecl16near FUN_1000_12c6(void)

{
  byte bVar1;
  undefined2 uVar2;
  int iVar3;
  undefined1 in_BL;
  undefined2 unaff_DS;
  
  iVar3 = -1;
  do {
    bVar1 = in(*(undefined2 *)0x8c);
    if ((bVar1 & 0x40) == 0) {
      out(*(undefined2 *)0x8a,in_BL);
      uVar2 = 1;
      goto LAB_1000_12e7;
    }
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  uVar2 = 0;
LAB_1000_12e7:
  while (bVar1 = in(*(undefined2 *)0x8c), (bVar1 & 0x80) == 0) {
    in(*(undefined2 *)0x8a);
  }
  return uVar2;
}



void __cdecl16near FUN_1000_12fd(void)

{
  char *in_AX;
  char *pcVar1;
  undefined2 unaff_DS;
  
  for (pcVar1 = (char *)0x90; *pcVar1 != -1; pcVar1 = pcVar1 + 1) {
    FUN_1000_12c6();
  }
  *(undefined1 *)0x80 = 0;
  for (; *in_AX != -1; in_AX = in_AX + 1) {
    FUN_1000_12bc();
  }
  FUN_1000_12c6();
  FUN_1000_12c6();
  FUN_1000_127a();
  return;
}



void __cdecl16near FUN_1000_1395(void)

{
  FUN_1000_12c6();
  return;
}



void __cdecl16near FUN_1000_1924(byte param_1,byte param_2,byte param_3)

{
  DAT_1322_0099 = param_1 & 3;
  DAT_1322_009a = param_2 & 7;
  DAT_1322_009b = param_3 & 7;
  FUN_1000_12fd();
  return;
}



undefined2 __cdecl16far FUN_1000_19bc(uint param_1)

{
  undefined2 uVar1;
  
  uVar1 = 0x1322;
  if (param_1 < 0x8020) {
    if (param_1 < 0x40) {
      if (param_1 < 0x20) {
        if (param_1 <= DAT_1322_00f8) {
          uVar1 = (*(code *)*(undefined2 *)(param_1 * 2 + 0x2a5c))();
        }
      }
      else if (param_1 <= DAT_1322_00fa) {
        uVar1 = (*(code *)*(undefined2 *)((param_1 - 0x20) * 2 + 0x2a6e))();
        return uVar1;
      }
    }
    else if (param_1 <= DAT_1322_00fc) {
      uVar1 = (*(code *)*(undefined2 *)((param_1 - 0x40) * 2 + 0x2ac4))();
      return uVar1;
    }
  }
  else if (param_1 <= DAT_1322_00fe) {
    uVar1 = (*(code *)*(undefined2 *)((param_1 + 0x7fe0) * 2 + 0x2ab6))();
    return uVar1;
  }
  return uVar1;
}



void __cdecl16near FUN_1000_1d9b(void)

{
  FUN_1000_1dd2();
  in(0x21);
  in(0x21);
  FUN_1000_1dd2();
  return;
}



void __cdecl16near FUN_1000_1dae(void)

{
  byte bVar1;
  undefined2 in_DX;
  
  do {
    bVar1 = in(in_DX);
  } while ((bVar1 & 0xc0) != 0);
  return;
}



undefined2 __cdecl16near FUN_1000_1db4(void)

{
  undefined2 in_AX;
  int iVar1;
  undefined2 extraout_DX;
  undefined2 unaff_DS;
  byte in_CF;
  byte in_PF;
  byte in_AF;
  byte in_ZF;
  byte in_SF;
  byte in_TF;
  byte in_IF;
  byte in_OF;
  byte in_NT;
  
  iVar1 = *(int *)0x1b26 + 2;
  out(iVar1,0xff);
  out(iVar1,(char)((uint)in_AX >> 8));
  out(*(int *)0x1b26 + 3,(char)in_AX);
  FUN_1000_1dae((uint)(in_NT & 1) * 0x4000 | (uint)(in_OF & 1) * 0x800 | (uint)(in_IF & 1) * 0x200 |
                (uint)(in_TF & 1) * 0x100 | (uint)(in_SF & 1) * 0x80 | (uint)(in_ZF & 1) * 0x40 |
                (uint)(in_AF & 1) * 0x10 | (uint)(in_PF & 1) * 4 | (uint)(in_CF & 1));
  out(extraout_DX,0xfe);
  return 0xfe;
}



void __cdecl16near FUN_1000_1dd2(void)

{
  undefined1 in_AL;
  undefined1 in_AH;
  uint in_DX;
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = (in_DX & 0xff) * 2 + 4 + *(int *)0x1b26;
  out(iVar1,in_AH);
  in(0x21);
  in(0x21);
  out(iVar1 + 1,in_AL);
  return;
}



void __cdecl16near FUN_1000_1ded(void)

{
  char cVar1;
  undefined1 in_AH;
  int iVar2;
  undefined2 in_DX;
  
  iVar2 = 500;
  do {
    cVar1 = in(in_DX);
    if (-1 < cVar1) {
      out(in_DX,in_AH);
      return;
    }
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  return;
}



char __cdecl16near FUN_1000_1e01(void)

{
  char cVar1;
  char cVar2;
  int iVar3;
  undefined1 uVar4;
  undefined2 unaff_DS;
  
  cVar2 = (char)*(undefined2 *)0x1b26;
  uVar4 = (undefined1)((uint)*(undefined2 *)0x1b26 >> 8);
  iVar3 = 0x200;
  do {
    cVar1 = in(CONCAT11(uVar4,cVar2 + '\x0e'));
    if (cVar1 < '\0') {
      cVar2 = in(CONCAT11(uVar4,cVar2 + '\n'));
      return cVar2;
    }
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  return cVar1;
}



void __cdecl16near FUN_1000_1e1f(void)

{
  undefined1 in_AL;
  char cVar1;
  undefined2 in_DX;
  
  do {
    cVar1 = in(in_DX);
  } while (cVar1 < '\0');
  out(in_DX,in_AL);
  return;
}



undefined1 __cdecl16near FUN_1000_1e2a(void)

{
  char cVar1;
  undefined1 uVar2;
  char cVar3;
  undefined2 unaff_DS;
  
  cVar3 = (char)*(undefined2 *)0x1b26;
  uVar2 = (undefined1)((uint)*(undefined2 *)0x1b26 >> 8);
  do {
    cVar1 = in(CONCAT11(uVar2,cVar3 + '\x0e'));
  } while (-1 < cVar1);
  uVar2 = in(CONCAT11(uVar2,cVar3 + '\n'));
  return uVar2;
}



undefined2 __cdecl16near FUN_1000_1e3d(void)

{
  char cVar1;
  undefined2 uVar2;
  char cVar3;
  undefined2 unaff_DS;
  
  if (((*(int *)0x1b24 == 0x12) || (*(int *)0x1b24 == 0x18)) || (*(int *)0x1b24 == 0x20)) {
    uVar2 = CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\x06');
    out(uVar2,1);
    in(uVar2);
    in(uVar2);
    in(uVar2);
    in(uVar2);
    out(uVar2,0);
    cVar3 = '@';
    do {
      cVar1 = FUN_1000_1e01();
      if (cVar1 == -0x56) goto LAB_1000_1ed9;
      cVar3 = cVar3 + -1;
    } while (cVar3 != '\0');
    uVar2 = 2;
  }
  else {
    if (*(int *)0x1b24 == 0x22) {
      FUN_1000_1dd2();
      FUN_1000_1dd2();
      FUN_1000_1dd2();
      FUN_1000_1d9b();
      FUN_1000_1d9b();
      in(*(int *)0x1b26 + 4);
      *(undefined1 *)0x1b55 = 0;
      FUN_1000_1dd2();
      *(byte *)0x1b56 = (byte)*(undefined2 *)0x1b53 | 0x66;
      FUN_1000_1dd2();
      FUN_1000_1dd2();
      FUN_1000_1db4();
      FUN_1000_1db4();
    }
LAB_1000_1ed9:
    uVar2 = 0;
  }
  return uVar2;
}



undefined2 __cdecl16near FUN_1000_1ede(void)

{
  undefined2 uVar1;
  int iVar2;
  undefined2 unaff_DS;
  
  *(undefined1 *)0x1b39 = 1;
  uVar1 = 0;
  if (*(int *)0x1b24 != 0x22) {
    *(undefined1 *)0x1b39 = 0;
    FUN_1000_2043();
    FUN_1000_202f();
    *(undefined1 *)0x1b38 = 0x58;
    FUN_1000_2489();
    uVar1 = 0;
    iVar2 = -0x1000;
    do {
      if (*(char *)0x1b39 != '\0') goto LAB_1000_1f1d;
      iVar2 = iVar2 + -1;
    } while (iVar2 != 0);
    uVar1 = 3;
  }
LAB_1000_1f1d:
  FUN_1000_224a();
  return uVar1;
}



undefined2 __cdecl16near FUN_1000_1f25(void)

{
  uint uVar1;
  undefined2 unaff_DS;
  
  if (((*(int *)0x1b24 == 0x12) || (*(int *)0x1b24 == 0x18)) || (*(int *)0x1b24 == 0x20)) {
    FUN_1000_1e1f();
    FUN_1000_1e2a();
    uVar1 = FUN_1000_1e2a();
    *(uint *)0x1c79 = uVar1;
    if (uVar1 < 0x103) {
      return 1;
    }
  }
  return 0;
}



undefined2 __cdecl16near FUN_1000_1f60(void)

{
  char cVar1;
  undefined2 uVar2;
  undefined1 uVar3;
  int iVar4;
  undefined2 uVar5;
  undefined2 unaff_DS;
  byte in_CF;
  byte in_PF;
  byte in_AF;
  byte in_ZF;
  byte in_SF;
  byte in_TF;
  byte in_IF;
  byte in_OF;
  byte in_NT;
  
  uVar2 = *(undefined2 *)0x1b24;
  cVar1 = (char)uVar2;
  if (((cVar1 == '\x12') || (cVar1 == '\x18')) || (cVar1 == ' ')) {
    uVar2 = CONCAT11(0xd0,cVar1);
    uVar5 = CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\f');
    iVar4 = 0x1000;
    do {
      if (*(char *)0x1b3a == '\0') {
        return uVar2;
      }
      cVar1 = in(uVar5);
      uVar2 = CONCAT11((char)((uint)uVar2 >> 8),cVar1);
      if (cVar1 < '\0') {
        do {
          uVar3 = (undefined1)((uint)uVar2 >> 8);
          cVar1 = in(uVar5);
          uVar2 = CONCAT11(uVar3,cVar1);
        } while (cVar1 < '\0');
        out(uVar5,uVar3);
        return CONCAT11(uVar3,uVar3);
      }
      iVar4 = iVar4 + -1;
    } while (iVar4 != 0);
  }
  else {
    uVar3 = (undefined1)((uint)uVar2 >> 8);
    if ((cVar1 == '\x19') || (cVar1 == '!')) {
      uVar2 = CONCAT11(uVar3,0xb9);
      out(0xb8a,0xb9);
    }
    else if (cVar1 == '\x11') {
      uVar2 = CONCAT11(uVar3,0xb0);
      out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\v'),0xb0
         );
    }
    else if (cVar1 == '\"') {
      *(byte *)0x1b55 = *(byte *)0x1b55 | 2;
      FUN_1000_1dd2((uint)(in_NT & 1) * 0x4000 | (uint)(in_OF & 1) * 0x800 |
                    (uint)(in_IF & 1) * 0x200 | (uint)(in_TF & 1) * 0x100 | (uint)(in_SF & 1) * 0x80
                    | (uint)(in_ZF & 1) * 0x40 | (uint)(in_AF & 1) * 0x10 | (uint)(in_PF & 1) * 4 |
                    (uint)(in_CF & 1));
      *(byte *)0x1b56 = *(byte *)0x1b56 & 0xfe;
      uVar2 = FUN_1000_1dd2();
    }
  }
  return uVar2;
}



undefined4 __cdecl16near FUN_1000_1fde(void)

{
  undefined2 in_AX;
  uint uVar1;
  undefined1 in_CL;
  undefined1 in_CH;
  undefined2 in_DX;
  undefined2 unaff_DS;
  
  out(*(undefined2 *)0x1b30,*(byte *)0x1b29 & 3 | 4);
  uVar1 = CONCAT11(*(undefined1 *)0x1b29,(char)((uint)in_DX >> 8)) & 0x3ff;
  out(*(undefined2 *)0x1b32,(byte)uVar1 | (byte)(uVar1 >> 8));
  out(*(undefined2 *)0x1b2a,(char)in_DX);
  out(*(undefined2 *)0x1b34,(char)in_DX);
  out(*(undefined2 *)0x1b2c,(char)in_AX);
  out(*(undefined2 *)0x1b2c,(char)((uint)in_AX >> 8));
  out(*(undefined2 *)0x1b2e,in_CL);
  out(*(undefined2 *)0x1b2e,in_CH);
  out(*(undefined2 *)0x1b30,*(byte *)0x1b29 & 3);
  return CONCAT22(in_DX,in_AX);
}



undefined4 __cdecl16near FUN_1000_202f(void)

{
  uint in_AX;
  uint in_DX;
  
  return CONCAT22((in_DX >> 0xc) + (uint)CARRY2(in_AX,in_DX * 0x10),in_AX + in_DX * 0x10);
}



undefined4 __cdecl16near FUN_1000_2043(void)

{
  byte bVar1;
  undefined2 in_AX;
  uint uVar2;
  byte bVar3;
  undefined2 in_DX;
  int iVar4;
  undefined2 *puVar5;
  undefined2 unaff_DS;
  
  if (*(int *)0x1b36 == 0) {
    iVar4 = 8;
    bVar3 = *(byte *)0x1b28;
    uVar2 = (uint)bVar3;
    if (7 < bVar3) {
      uVar2 = bVar3 & 0xff07;
      iVar4 = 0x70;
    }
    puVar5 = (undefined2 *)((iVar4 + uVar2) * 4);
    *(undefined2 *)0x1b3e = *puVar5;
    *puVar5 = in_AX;
    *(undefined2 *)0x1b40 = puVar5[1];
    puVar5[1] = 0x1000;
    bVar3 = ~('\x01' << (*(byte *)0x1b28 & 7));
    if (*(byte *)0x1b28 < 8) {
      bVar1 = in(0x21);
      out(0x21,bVar1 & bVar3);
    }
    else {
      bVar1 = in(0xa1);
      out(0xa1,bVar1 & bVar3);
    }
    *(undefined2 *)0x1b36 = 1;
  }
  return CONCAT22(in_DX,in_AX);
}



void __cdecl16near FUN_1000_20b1(void)

{
  byte bVar1;
  byte bVar3;
  uint uVar2;
  int iVar4;
  undefined2 *puVar5;
  undefined2 unaff_DS;
  
  if (*(int *)0x1b36 != 0) {
    bVar3 = '\x01' << (*(byte *)0x1b28 & 7);
    if (*(byte *)0x1b28 < 8) {
      bVar1 = in(0x21);
      out(0x21,bVar1 | bVar3);
    }
    else {
      bVar1 = in(0xa1);
      out(0xa1,bVar1 | bVar3);
    }
    iVar4 = 8;
    bVar3 = *(byte *)0x1b28;
    uVar2 = (uint)bVar3;
    if (7 < bVar3) {
      uVar2 = bVar3 & 0xff07;
      iVar4 = 0x70;
    }
    puVar5 = (undefined2 *)((iVar4 + uVar2) * 4);
    *puVar5 = *(undefined2 *)0x1b3e;
    puVar5[1] = *(undefined2 *)0x1b40;
    *(undefined1 *)0x1b3a = 0;
    *(undefined2 *)0x1b36 = 0;
    *(undefined1 *)0x1b52 = 0;
    *(undefined2 *)0x1c77 = 0;
  }
  return;
}



void __cdecl16near FUN_1000_224a(void)

{
  undefined1 uVar1;
  undefined2 unaff_DS;
  
  out(*(undefined2 *)0x1b30,(*(byte *)0x1b29 & 3) + 4);
  if (((*(int *)0x1b24 == 0x12) || (*(int *)0x1b24 == 0x18)) || (*(int *)0x1b24 == 0x20)) {
    in(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\x0e'));
  }
  else if (*(int *)0x1b24 == 0x11) {
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\v'),0xb0);
  }
  else if ((*(int *)0x1b24 == 0x19) || (*(int *)0x1b24 == 0x21)) {
    out(0xb8a,0x39);
    out(0xf8a,0x39);
    out(0xb8b,0);
    uVar1 = in(0xb89);
    out(0xb89,uVar1);
  }
  else if (*(int *)0x1b24 == 0x22) {
    FUN_1000_1d9b();
    FUN_1000_1d9b();
  }
  FUN_1000_20b1();
  *(undefined1 *)0x1b3a = 0;
  return;
}



undefined2 __cdecl16near FUN_1000_22ce(void)

{
  char cVar1;
  undefined2 unaff_DS;
  
  cVar1 = (char)*(undefined2 *)0x1b24;
  if (((cVar1 == '\x12') || (cVar1 == '\x18')) || (cVar1 == ' ')) {
    FUN_1000_1ded();
  }
  else if ((cVar1 == '\x19') || (cVar1 == '!')) {
    out(0xb8a,0x39);
  }
  return 0;
}



void __cdecl16near FUN_1000_2320(void)

{
  FUN_1000_202f();
  return;
}



undefined4 __cdecl16near FUN_1000_232b(void)

{
  undefined2 in_AX;
  undefined2 in_DX;
  undefined2 unaff_DS;
  
  *(uint *)0x2316 =
       CONCAT11((char)*(undefined2 *)0x1b48,(char)((uint)*(undefined2 *)0x1b46 >> 8)) >> 6;
  return CONCAT22(in_DX,in_AX);
}



undefined4 __cdecl16near FUN_1000_2341(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  code *pcVar1;
  undefined2 in_stack_00000000;
  
  pcVar1 = (code *)swi(0x67);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x67);
  (*pcVar1)();
  return CONCAT22(in_stack_00000000,param_3);
}



undefined4 __cdecl16near FUN_1000_2365(undefined2 param_1)

{
  code *pcVar1;
  undefined2 in_BX;
  
  pcVar1 = (code *)swi(0x67);
  (*pcVar1)();
  return CONCAT22(in_BX,param_1);
}



undefined2 __cdecl16near FUN_1000_237a(void)

{
  uint uVar1;
  undefined2 in_AX;
  uint uVar2;
  uint uVar3;
  int iVar4;
  int in_CX;
  uint in_BX;
  undefined2 unaff_DS;
  
  uVar2 = FUN_1000_232b();
  if ((((in_CX != 0) || ((in_BX & 0xe000) != 0)) || (CARRY2(in_BX,uVar2 & 0x3fff))) ||
     ((in_BX + (uVar2 & 0x3fff) & 0xc000) != 0)) {
    *(undefined1 *)0x1b52 = 1;
    uVar2 = *(uint *)0x1b4a & 0x3fff ^ 0x3fff;
  }
  else {
    *(undefined1 *)0x1b52 = 0;
    uVar2 = *(int *)0x1b4e - 1;
  }
  uVar2 = uVar2 & 0x1fff;
  uVar3 = *(uint *)0x1b4a;
  uVar1 = uVar3 + uVar2;
  *(int *)0x1b4a = uVar1 + 1;
  uVar3 = *(int *)0x1b4c + (uint)(CARRY2(uVar3,uVar2) || 0xfffe < uVar1);
  *(uint *)0x1b4c = uVar3;
  if ((*(uint *)0x231e < uVar3) ||
     ((*(uint *)0x231e <= uVar3 && (*(uint *)0x231c <= *(uint *)0x1b4a)))) {
    *(undefined2 *)0x1b4a = *(undefined2 *)0x2318;
    *(undefined2 *)0x1b4c = *(undefined2 *)0x231a;
  }
  uVar3 = *(uint *)0x1b4e;
  iVar4 = uVar3 - uVar2;
  *(int *)0x1b4e = iVar4 + -1;
  iVar4 = *(int *)0x1b50 - (uint)(uVar3 < uVar2 || iVar4 == 0);
  *(int *)0x1b50 = iVar4;
  uVar3 = *(uint *)0x1b46;
  uVar1 = uVar3 + uVar2;
  *(int *)0x1b46 = uVar1 + 1;
  *(int *)0x1b48 = *(int *)0x1b48 + (uint)(CARRY2(uVar3,uVar2) || 0xfffe < uVar1);
  if (iVar4 == 0 && *(int *)0x1b4e == 0) {
    *(undefined1 *)0x1b52 = 0;
  }
  return in_AX;
}



undefined2 __cdecl16near FUN_1000_241b(void)

{
  byte bVar1;
  undefined2 uVar2;
  char cVar3;
  uint uVar4;
  undefined2 uVar5;
  int iVar6;
  int iVar7;
  undefined1 uVar8;
  uint extraout_DX;
  undefined2 *puVar9;
  undefined2 unaff_DS;
  bool bVar10;
  byte in_AF;
  bool bVar11;
  byte in_TF;
  byte in_IF;
  bool bVar12;
  byte in_NT;
  undefined4 uVar13;
  
  puVar9 = (undefined2 *)*(undefined2 *)0x1c75;
  iVar7 = puVar9[2];
  iVar6 = puVar9[3];
  if (iVar7 == 0 && iVar6 == 0) {
    *(undefined1 *)0x1b52 = 0;
    return 0;
  }
  uVar5 = *puVar9;
  uVar2 = puVar9[1];
  *puVar9 = 0;
  puVar9[1] = 0;
  puVar9[2] = 0;
  puVar9[3] = 0;
  puVar9 = puVar9 + 4;
  if (puVar9 == (undefined2 *)0x1c71) {
    puVar9 = (undefined2 *)0x1b71;
  }
  *(undefined2 *)0x1c75 = puVar9;
  *(int *)0x1c77 = *(int *)0x1c77 + 1;
  *(undefined2 *)0x1b46 = uVar5;
  *(undefined2 *)0x1b48 = uVar2;
  uVar13 = FUN_1000_2320();
  *(undefined2 *)0x1b4a = (int)uVar13;
  *(undefined2 *)0x1b4c = (int)((ulong)uVar13 >> 0x10);
  *(int *)0x1b4e = iVar7;
  *(int *)0x1b50 = iVar6;
  uVar4 = FUN_1000_237a();
  FUN_1000_2043(uVar4);
  bVar1 = *(byte *)0x1b29;
  bVar10 = bVar1 < 4;
  bVar12 = SBORROW1(bVar1,'\x04');
  cVar3 = bVar1 - 4;
  bVar11 = cVar3 == '\0';
  bVar1 = POPCOUNT(cVar3);
  if (!bVar10) {
    iVar6 = (iVar6 + 1U >> 1) - 1;
    bVar11 = extraout_DX >> 1 == 0;
    bVar1 = POPCOUNT(extraout_DX >> 1 & 0xff);
    bVar12 = (int)uVar4 < 0 != ((extraout_DX & 1) != 0);
  }
  uVar13 = FUN_1000_2341((uint)(in_NT & 1) * 0x4000 | (uint)bVar12 * 0x800 |
                         (uint)(in_IF & 1) * 0x200 | (uint)(in_TF & 1) * 0x100 |
                         (uint)(bVar10 && cVar3 < '\0') * 0x80 | (uint)bVar11 * 0x40 |
                         (uint)(in_AF & 1) * 0x10 | (uint)((bVar1 & 1) == 0) * 4 |
                         (uint)(bVar10 || (uVar4 & 1) != 0));
  if (*(int *)0x1b24 == 0x11) {
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\v'),0xb6);
    uVar5 = CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\n');
    out(uVar5,(char)*(undefined2 *)0x1b53);
    out(uVar5,(char)((uint)*(undefined2 *)0x1b53 >> 8));
  }
  else if ((*(int *)0x1b24 == 0x19) || (*(int *)0x1b24 == 0x21)) {
    out(0xf8a,0xb9);
  }
  else if (*(int *)0x1b24 == 0x22) {
    if (*(char *)0x1b3a == '\0') {
      FUN_1000_1d9b((int)((ulong)uVar13 >> 0x10),iVar6,(int)uVar13);
      *(undefined1 *)0x1b55 = 0x17;
      FUN_1000_1dd2();
      iVar7 = 8;
      do {
        FUN_1000_1dd2();
        iVar7 = iVar7 + -1;
      } while (iVar7 != 0);
    }
    *(byte *)0x1b56 = (byte)*(undefined2 *)0x1b53 | 0x66;
    FUN_1000_1dd2();
  }
  FUN_1000_1fde();
  cVar3 = (char)*(undefined2 *)0x1b24;
  if (((cVar3 == '\x12') || (cVar3 == '\x18')) || (cVar3 == ' ')) {
    FUN_1000_1e1f();
    FUN_1000_1e1f();
    FUN_1000_1e1f();
    FUN_1000_1e1f();
    FUN_1000_1e1f();
  }
  else if (cVar3 == '\x11') {
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\f'),0);
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\x0e'),0);
  }
  else if ((cVar3 == '\x19') || (cVar3 == '!')) {
    out(0xf88,0x80);
    out(0x138b,0x36);
    out(5000,(char)*(undefined2 *)0x1b53);
    out(5000,(char)((uint)*(undefined2 *)0x1b53 >> 8));
    out(0x138b,0x74);
    out(0x1389,(char)(iVar6 + 1));
    uVar8 = (undefined1)((uint)(iVar6 + 1) >> 8);
    out(0x1389,uVar8);
    out(0xb89,uVar8);
    in(0xb89);
    out(0xb8b,8);
    out(0xf8a,0xb9);
    out(0xf8a,0xf9);
    out(0xb8a,0xf9);
  }
  else {
    if (cVar3 != '\"') goto LAB_1000_2605;
    *(byte *)0x1b56 = *(byte *)0x1b56 | 1;
    FUN_1000_1dd2();
    *(byte *)0x1b55 = *(byte *)0x1b55 & 0xfd;
    FUN_1000_1dd2();
  }
  *(undefined1 *)0x1b3a = 0xff;
LAB_1000_2605:
  uVar5 = FUN_1000_2365();
  return uVar5;
}



undefined2 __cdecl16near FUN_1000_2489(void)

{
  byte bVar1;
  char cVar2;
  uint in_AX;
  undefined2 uVar3;
  int in_CX;
  int iVar4;
  undefined1 uVar5;
  uint in_DX;
  undefined2 unaff_DS;
  bool bVar6;
  byte in_AF;
  bool bVar7;
  byte in_TF;
  byte in_IF;
  bool bVar8;
  byte in_NT;
  undefined4 uVar9;
  
  bVar1 = *(byte *)0x1b29;
  bVar6 = bVar1 < 4;
  bVar8 = SBORROW1(bVar1,'\x04');
  cVar2 = bVar1 - 4;
  bVar7 = cVar2 == '\0';
  bVar1 = POPCOUNT(cVar2);
  if (!bVar6) {
    in_CX = (in_CX + 1U >> 1) - 1;
    bVar7 = in_DX >> 1 == 0;
    bVar1 = POPCOUNT(in_DX >> 1 & 0xff);
    bVar8 = (int)in_AX < 0 != ((in_DX & 1) != 0);
  }
  uVar9 = FUN_1000_2341((uint)(in_NT & 1) * 0x4000 | (uint)bVar8 * 0x800 | (uint)(in_IF & 1) * 0x200
                        | (uint)(in_TF & 1) * 0x100 | (uint)(bVar6 && cVar2 < '\0') * 0x80 |
                        (uint)bVar7 * 0x40 | (uint)(in_AF & 1) * 0x10 | (uint)((bVar1 & 1) == 0) * 4
                        | (uint)(bVar6 || (in_AX & 1) != 0));
  if (*(int *)0x1b24 == 0x11) {
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\v'),0xb6);
    uVar3 = CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\n');
    out(uVar3,(char)*(undefined2 *)0x1b53);
    out(uVar3,(char)((uint)*(undefined2 *)0x1b53 >> 8));
  }
  else if ((*(int *)0x1b24 == 0x19) || (*(int *)0x1b24 == 0x21)) {
    out(0xf8a,0xb9);
  }
  else if (*(int *)0x1b24 == 0x22) {
    if (*(char *)0x1b3a == '\0') {
      FUN_1000_1d9b((int)((ulong)uVar9 >> 0x10),in_CX,(int)uVar9);
      *(undefined1 *)0x1b55 = 0x17;
      FUN_1000_1dd2();
      iVar4 = 8;
      do {
        FUN_1000_1dd2();
        iVar4 = iVar4 + -1;
      } while (iVar4 != 0);
    }
    *(byte *)0x1b56 = (byte)*(undefined2 *)0x1b53 | 0x66;
    FUN_1000_1dd2();
  }
  FUN_1000_1fde();
  cVar2 = (char)*(undefined2 *)0x1b24;
  if (((cVar2 == '\x12') || (cVar2 == '\x18')) || (cVar2 == ' ')) {
    FUN_1000_1e1f();
    FUN_1000_1e1f();
    FUN_1000_1e1f();
    FUN_1000_1e1f();
    FUN_1000_1e1f();
  }
  else if (cVar2 == '\x11') {
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\f'),0);
    out(CONCAT11((char)((uint)*(undefined2 *)0x1b26 >> 8),(char)*(undefined2 *)0x1b26 + '\x0e'),0);
  }
  else if ((cVar2 == '\x19') || (cVar2 == '!')) {
    out(0xf88,0x80);
    out(0x138b,0x36);
    out(5000,(char)*(undefined2 *)0x1b53);
    out(5000,(char)((uint)*(undefined2 *)0x1b53 >> 8));
    out(0x138b,0x74);
    out(0x1389,(char)(in_CX + 1));
    uVar5 = (undefined1)((uint)(in_CX + 1) >> 8);
    out(0x1389,uVar5);
    out(0xb89,uVar5);
    in(0xb89);
    out(0xb8b,8);
    out(0xf8a,0xb9);
    out(0xf8a,0xf9);
    out(0xb8a,0xf9);
  }
  else {
    if (cVar2 != '\"') goto LAB_1000_2605;
    *(byte *)0x1b56 = *(byte *)0x1b56 | 1;
    FUN_1000_1dd2();
    *(byte *)0x1b55 = *(byte *)0x1b55 & 0xfd;
    FUN_1000_1dd2();
  }
  *(undefined1 *)0x1b3a = 0xff;
LAB_1000_2605:
  uVar3 = FUN_1000_2365();
  return uVar3;
}



int __cdecl16near FUN_1000_260c(void)

{
  uint in_AX;
  uint uVar1;
  uint in_DX;
  uint *puVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  iVar3 = 0;
  puVar2 = (uint *)0x1c7b;
  while( true ) {
    uVar1 = puVar2[1] + puVar2[3] + (uint)CARRY2(*puVar2,puVar2[2]);
    if ((in_DX < uVar1) || ((in_DX <= uVar1 && (in_AX < *puVar2 + puVar2[2])))) break;
    iVar3 = iVar3 + 1;
    puVar2 = puVar2 + 4;
  }
  return iVar3;
}



int __cdecl16far FUN_1000_2635(undefined2 param_1,uint param_2,undefined1 param_3,uint param_4)

{
  int iVar1;
  undefined1 uVar2;
  undefined4 uVar3;
  
  DAT_1000_1b39 = 0;
  iVar1 = 4;
  if ((0x10 < param_2) && (param_2 < 0x23)) {
    DAT_1000_1b24 = param_2;
    if ((param_2 == 0x11) || (((param_2 == 0x12 || (param_2 == 0x18)) || (param_2 == 0x20)))) {
      DAT_1000_1b26 = param_1;
    }
    DAT_1000_1b28 = param_3;
    DAT_1000_1b29 = (undefined1)param_4;
    DAT_1000_1b2a = (uint)*(byte *)(param_4 + 6999);
    DAT_1000_1b2c = (uint)*(byte *)(param_4 + 0x1b5f);
    DAT_1000_1b2e = (uint)*(byte *)(param_4 + 0x1b67);
    if (param_4 < 4) {
      DAT_1000_1b30 = 10;
      DAT_1000_1b32 = 0xb;
      DAT_1000_1b34 = 0xc;
    }
    else {
      DAT_1000_1b30 = 0xd4;
      DAT_1000_1b32 = 0xd6;
      DAT_1000_1b34 = 0xd8;
    }
    iVar1 = FUN_1000_2910();
    if (iVar1 == 0) {
      uVar2 = 1;
      uVar3 = FUN_1000_202f();
      DAT_1000_231a = (undefined2)((ulong)uVar3 >> 0x10);
      DAT_1000_2318 = (undefined2)uVar3;
      uVar3 = FUN_1000_202f();
      DAT_1000_231e = (undefined2)((ulong)uVar3 >> 0x10);
      DAT_1000_231c = (undefined2)uVar3;
      FUN_1000_22ce();
      FUN_1000_2749();
      iVar1 = FUN_1000_1e3d();
      if ((((bool)uVar2) && (iVar1 = FUN_1000_1f25(), (bool)uVar2)) &&
         (iVar1 = FUN_1000_1ede(), (bool)uVar2)) {
        FUN_1000_2749();
        DAT_1000_1b6f = FUN_1000_260c();
        DAT_1000_1c73 = 0x1b71;
        DAT_1000_1c75 = 0x1b71;
        FUN_1000_22ce();
        iVar1 = 0;
      }
    }
  }
  DAT_1000_1b38 = 0x48;
  return iVar1;
}



void __cdecl16far FUN_1000_2737(void)

{
  FUN_1000_2749();
  return;
}



undefined2 __cdecl16near FUN_1000_2749(void)

{
  char cVar1;
  undefined2 uVar2;
  uint uVar3;
  uint in_CX;
  undefined2 unaff_DS;
  
  uVar2 = 1;
  if (in_CX != 0) {
    cVar1 = (char)*(undefined2 *)0x1b24;
    if (((cVar1 == '\x12') || (cVar1 == '\x18')) || (cVar1 == ' ')) {
      uVar3 = ~(uint)(1000000 / (ulong)in_CX);
    }
    else if ((cVar1 == '\x19') || (cVar1 == '!')) {
      uVar3 = (uint)(0x1234dc / (ulong)in_CX);
    }
    else if (cVar1 == '\x11') {
      uVar3 = (uint)(0x6db5f0 / (ulong)in_CX);
    }
    else {
      if (cVar1 != '\"') {
        return *(undefined2 *)0x1b24;
      }
      uVar3 = 0x18;
      if (((8999 < in_CX) && (uVar3 = 0x10, 16999 < in_CX)) && (uVar3 = 8, 32999 < in_CX)) {
        uVar3 = 0;
      }
    }
    *(uint *)0x1b53 = uVar3;
    uVar2 = 0;
  }
  return uVar2;
}



uint __cdecl16far FUN_1000_27b4(uint param_1)

{
  undefined2 *puVar1;
  undefined2 *puVar2;
  uint uVar3;
  undefined2 *puVar4;
  int iVar5;
  
  uVar3 = DAT_1000_1c73[2] | DAT_1000_1c73[3];
  if ((uVar3 == 0) && (uVar3 = 1, param_1 < DAT_1000_1b6f)) {
    if (DAT_1000_1b3a != '\0') {
      FUN_1000_1f60();
      FUN_1000_224a();
    }
    FUN_1000_2749();
    puVar4 = (undefined2 *)(param_1 * 8 + 0x1c7b);
    for (iVar5 = 4; iVar5 != 0; iVar5 = iVar5 + -1) {
      puVar2 = DAT_1000_1c73;
      DAT_1000_1c73 = DAT_1000_1c73 + 1;
      puVar1 = puVar4;
      puVar4 = puVar4 + 1;
      *puVar2 = *puVar1;
    }
    if (DAT_1000_1c73 == (undefined2 *)0x1c71) {
      DAT_1000_1c73 = (undefined2 *)0x1b71;
    }
    if (DAT_1000_1b3a == '\0') {
      FUN_1000_241b();
    }
    uVar3 = 0;
  }
  return uVar3;
}



void __cdecl16near FUN_1000_2896(void)

{
  code *pcVar1;
  undefined2 in_BX;
  undefined2 unaff_DS;
  undefined1 in_ZF;
  
  DAT_1000_288a = 0;
  FUN_1000_28be();
  if ((bool)in_ZF) {
    pcVar1 = (code *)swi(0x67);
    (*pcVar1)();
    if ((bool)in_ZF) {
      pcVar1 = (code *)swi(0x67);
      (*pcVar1)();
      if ((bool)in_ZF) {
        pcVar1 = (code *)swi(0x67);
        DAT_1000_288a = in_BX;
        (*pcVar1)();
        if ((bool)in_ZF) {
          *(undefined2 *)0xe4 = in_BX;
        }
      }
    }
  }
  return;
}



void __cdecl16near FUN_1000_28be(void)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  char *pcVar4;
  char *pcVar5;
  undefined2 unaff_DS;
  
  pcVar5 = (char *)0xa;
  pcVar4 = (char *)0xa2;
  iVar3 = 8;
  do {
    if (iVar3 == 0) {
      return;
    }
    iVar3 = iVar3 + -1;
    pcVar2 = pcVar5;
    pcVar5 = pcVar5 + 1;
    pcVar1 = pcVar4;
    pcVar4 = pcVar4 + 1;
  } while (*pcVar1 == *pcVar2);
  return;
}



void __cdecl16near FUN_1000_28d8(int param_1)

{
  code *pcVar1;
  undefined2 uVar2;
  byte *pbVar3;
  
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  uVar2 = DAT_1000_288a;
  if ((DAT_1000_10f0 == 0x22) || (DAT_1000_10f0 == 0x39)) {
    pbVar3 = (byte *)0x0;
    do {
      *pbVar3 = *pbVar3 ^ 0x80;
      pbVar3 = pbVar3 + 1;
      param_1 = param_1 + -1;
    } while (param_1 != 0);
  }
  return;
}



undefined2 __cdecl16near FUN_1000_2910(void)

{
  code *pcVar1;
  undefined2 uVar2;
  uint uVar3;
  undefined1 in_CF;
  undefined1 in_ZF;
  ulong uVar4;
  
  FUN_1000_2896();
  if ((bool)in_ZF) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    if ((bool)in_CF) {
      uVar2 = 8;
    }
    else {
      pcVar1 = (code *)swi(0x21);
      uVar2 = (*pcVar1)();
      if ((bool)in_CF) {
        uVar2 = 8;
      }
      else {
        DAT_1000_288e = DAT_1322_00cf;
        DAT_1000_2890 = DAT_1322_00d1;
        DAT_1000_2892 = DAT_1322_00cf;
        DAT_1000_2894 = DAT_1322_00d1;
        pcVar1 = (code *)swi(0x67);
        DAT_1322_00e0 = uVar2;
        uVar4 = (*pcVar1)();
        DAT_1000_288c = (undefined2)(uVar4 >> 0x10);
        if ((uVar4 & 0xff00) == 0) {
          pcVar1 = (code *)swi(0x67);
          (*pcVar1)();
          DAT_1322_00e2 = 0;
          for (; (uVar3 = 0x4000, (DAT_1000_2892 & 0xc000) != 0 || DAT_1000_2894 != 0 ||
                 (uVar3 = DAT_1000_2892 & 0x3fff, uVar3 != 0));
              DAT_1000_2892 = DAT_1000_2892 - uVar3) {
            pcVar1 = (code *)swi(0x67);
            (*pcVar1)();
            FUN_1000_28d8();
            DAT_1000_2894 = DAT_1000_2894 - (uint)(DAT_1000_2892 < uVar3);
            DAT_1322_00e2 = DAT_1322_00e2 + 1;
          }
          pcVar1 = (code *)swi(0x67);
          (*pcVar1)();
          pcVar1 = (code *)swi(0x21);
          (*pcVar1)();
          uVar2 = 0;
        }
        else {
          uVar2 = 7;
        }
      }
    }
  }
  else {
    uVar2 = 5;
  }
  return uVar2;
}


