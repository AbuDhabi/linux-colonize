typedef unsigned char   undefined;

typedef unsigned int    uint;
typedef unsigned char    undefined1;
typedef unsigned int    undefined2;
typedef unsigned long    undefined4;
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



byte DAT_0000_0417;
undefined FUN_1000_0002;
undefined caseD_1f;
byte DAT_0000_0410;
undefined FUN_1000_0008;
undefined FUN_1000_000a;
char DAT_0000_0449;
int DAT_25e7_42d6;
undefined2 DAT_25e7_42d2;
undefined1 *DAT_25e7_42d4;
undefined2 DAT_25e7_42ce;
uint DAT_25e7_42cc;
undefined2 DAT_25e7_42ca;
undefined2 DAT_25e7_42c8;
undefined2 DAT_25e7_42c6;
undefined2 DAT_25e7_42c4;
undefined2 DAT_25e7_42c2;
undefined2 DAT_25e7_42c0;
uint DAT_25e7_42be;
char DAT_25e7_3c8e;
undefined1 DAT_25e7_3c8f;
uint DAT_25e7_3ca0;
undefined2 DAT_25e7_3ca2;
byte DAT_25e7_3c9c;
char DAT_25e7_5ad2;
uint DAT_25e7_5e22;
undefined2 DAT_25e7_5e24;
uint DAT_25e7_42bc;
undefined2 DAT_25e7_42ba;
undefined2 DAT_25e7_42b8;
undefined2 DAT_25e7_5e22;
char DAT_1ffe_0034;
int DAT_25e7_4532;
undefined2 DAT_25e7_4534;
undefined1 *DAT_25e7_4538;
undefined2 DAT_25e7_456e;

void __cdecl16far switchD_1000:bb3f::caseD_1f(void)

{
  return;
}



void __cdecl16far FUN_1000_0002(void)

{
  return;
}



void __cdecl16far FUN_1000_0004(void)

{
  return;
}



void __cdecl16far FUN_1000_0006(void)

{
  return;
}



void __cdecl16far FUN_1000_0008(void)

{
  return;
}



void __cdecl16far FUN_1000_000a(void)

{
  return;
}



void __cdecl16far FUN_1000_000c(void)

{
  return;
}



void __cdecl16far FUN_1000_000e(void)

{
  return;
}



void __cdecl16far FUN_1000_0010(void)

{
  return;
}



void __cdecl16far FUN_1000_0012(void)

{
  return;
}



void __cdecl16far FUN_1000_0014(void)

{
  return;
}



void __cdecl16far FUN_1000_0016(void)

{
  return;
}



void __cdecl16far FUN_1000_0018(void)

{
  byte bVar1;
  undefined2 unaff_DS;
  
  *(uint *)0x550 = (uint)*(byte *)0x95;
  *(uint *)0x552 = (uint)*(byte *)0x96;
  *(uint *)0x54a = (uint)*(byte *)0x92;
  *(uint *)0x54e = (uint)*(byte *)0x93;
  *(uint *)0x54c = (uint)*(byte *)0x94;
  bVar1 = *(byte *)0x97;
  *(uint *)0x542 = (uint)bVar1;
  *(uint *)0x540 = (uint)bVar1;
  *(uint *)0x544 = (uint)*(byte *)0x99;
  *(uint *)0x546 = (uint)*(byte *)0x9a;
  *(uint *)0x548 = (uint)*(byte *)0x9b;
  *(undefined2 *)0x56c = 0x49fa;
  *(undefined2 *)0x564 = 1;
  return;
}



undefined2 __cdecl16far FUN_1000_0060(void)

{
  bool bVar1;
  int iVar2;
  undefined2 unaff_DS;
  undefined2 local_a;
  
  bVar1 = true;
  local_a = 0;
  FUN_1d18_0006();
  FUN_1c21_002a();
  do {
    FUN_1c21_0042();
    iVar2 = FUN_1baf_0004();
    if (iVar2 != 0) {
      local_a = FUN_1baf_0018();
      bVar1 = false;
    }
    if (*(int *)0x730 != 0) {
      bVar1 = false;
    }
    FUN_1c21_0110();
  } while (bVar1);
  return local_a;
}



void __cdecl16far FUN_1000_00b6(void)

{
  uint uVar1;
  uint uVar2;
  undefined2 unaff_DS;
  undefined2 uVar3;
  undefined2 uVar4;
  undefined2 uVar5;
  undefined2 uVar6;
  undefined2 local_a;
  undefined2 local_8;
  
  local_8 = 0;
  do {
    if (*(int *)0x4b12 <= local_8) {
      return;
    }
    for (local_a = 0; local_a < *(int *)0x4b14; local_a = local_a + 1) {
      uVar1 = FUN_1c78_0000(*(undefined2 *)0x4e92,*(undefined2 *)0x4e94,*(undefined2 *)0x4e96,
                            *(undefined2 *)0x4e98);
      uVar2 = uVar1 & 0x1f;
      if (uVar2 < 0x18) {
        if ((uVar1 & 0x20) == 0) {
          if ((uVar2 < 0x10) || (0x17 < uVar2)) goto LAB_1000_00cc;
          FUN_1c78_0000(*(undefined2 *)0x4e92,*(undefined2 *)0x4e94,*(undefined2 *)0x4e96,
                        *(undefined2 *)0x4e98);
          uVar6 = *(undefined2 *)0x4e98;
          uVar5 = *(undefined2 *)0x4e96;
          uVar4 = *(undefined2 *)0x4e94;
          uVar3 = *(undefined2 *)0x4e92;
        }
        else {
          uVar6 = *(undefined2 *)0x4e98;
          uVar5 = *(undefined2 *)0x4e96;
          uVar4 = *(undefined2 *)0x4e94;
          uVar3 = *(undefined2 *)0x4e92;
        }
        FUN_1c76_0004(uVar3,uVar4,uVar5,uVar6);
      }
LAB_1000_00cc:
    }
    local_8 = local_8 + 1;
  } while( true );
}



byte __cdecl16far FUN_1000_0186(void)

{
  return DAT_0000_0417 & 3;
}



undefined2 __cdecl16far FUN_1000_0196(void)

{
  undefined2 uVar1;
  int iVar2;
  int in_DX;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 1;
  uVar1 = FUN_16d7_02fe(0x800,*(undefined2 *)0x80,*(undefined2 *)0x82);
  *(undefined2 *)0x78 = uVar1;
  *(int *)0x7a = in_DX;
  if ((in_DX != 0 || *(int *)0x78 != 0) && (iVar2 = FUN_1842_001a(0xa1,0x9c), iVar2 == 0)) {
    uVar1 = FUN_1842_0106(1,0);
    FUN_16d7_0642(*(undefined2 *)0x78,*(undefined2 *)0x7a,uVar1);
    uVar1 = FUN_1842_0106(0x1a);
    FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,1,uVar1);
    uVar1 = FUN_1842_0106(0x13);
    FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,1,uVar1);
    uVar1 = FUN_1842_0106(0x1b);
    FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,1,uVar1);
    uVar1 = FUN_1842_0106(0x14);
    FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,1,uVar1);
    FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,1,0xa9);
    uVar1 = FUN_1842_0106(0x1f);
    FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,1,uVar1);
    iVar2 = FUN_1842_001a(0xaf,0xaa);
    if (iVar2 == 0) {
      uVar1 = FUN_1842_0106(2,0);
      FUN_16d7_0642(*(undefined2 *)0x78,*(undefined2 *)0x7a,uVar1);
      uVar1 = FUN_1842_0106(0x24);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      uVar1 = FUN_1842_0106(0x25);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,0xb7);
      uVar1 = FUN_1842_0106(0x26);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      uVar1 = FUN_1842_0106(0x27);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      uVar1 = FUN_1842_0106(0x28);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      uVar1 = FUN_1842_0106(0x29);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,0xb8);
      uVar1 = FUN_1842_0106(0x2b);
      FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,2,uVar1);
      iVar2 = FUN_1842_001a(0xbd,0xb9);
      if (iVar2 == 0) {
        uVar1 = FUN_1842_0106(6,0);
        FUN_16d7_0642(*(undefined2 *)0x78,*(undefined2 *)0x7a,uVar1);
        uVar1 = FUN_1842_0106(0x4b);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,uVar1);
        uVar1 = FUN_1842_0106(0x4c);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,uVar1);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,0xc5);
        uVar1 = FUN_1842_0106(0x4d);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,uVar1);
        uVar1 = FUN_1842_0106(0x4a);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,uVar1);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,0xc6);
        uVar1 = FUN_1842_0106(0x4e);
        FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,6,uVar1);
        iVar2 = FUN_1842_001a(0xcc,199);
        if (iVar2 == 0) {
          uVar1 = FUN_1842_0106(7,1);
          FUN_16d7_0642(*(undefined2 *)0x78,*(undefined2 *)0x7a,uVar1);
          uVar1 = FUN_1842_0106(0x51);
          FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,7,uVar1);
          uVar1 = FUN_1842_0106(0x52);
          FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,7,uVar1);
          uVar1 = FUN_1842_0106(0x53);
          FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,7,uVar1);
          uVar1 = FUN_1842_0106(0x54);
          FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,7,uVar1);
          FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,7,0xd4);
          uVar1 = FUN_1842_0106(0x5f);
          FUN_16d7_07de(*(undefined2 *)0x78,*(undefined2 *)0x7a,7,uVar1);
          local_4 = 0;
        }
      }
    }
  }
  return local_4;
}



int __cdecl16far FUN_1000_056a(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined1 local_42 [30];
  undefined1 local_24 [14];
  int local_16;
  int local_14;
  int local_12;
  int local_10;
  int local_e;
  int local_c;
  int local_a;
  int local_8;
  int local_6;
  int local_4;
  
  local_14 = -1;
  local_e = 1;
  local_a = 0;
  local_c = 0;
  local_4 = 0;
  *(undefined2 *)0x521c = 0;
  iVar1 = FUN_2388_0abf(0x1000,param_3,0,local_42);
  while (iVar1 == 0) {
    *(int *)0x521c = *(int *)0x521c + 1;
    iVar1 = FUN_2388_0ab4(0x2388,local_42);
  }
  uVar2 = 0x2388;
  if (*(int *)0x521c != 0) {
    *(int *)0x4e78 = (*(int *)0x521c + 9) / 10;
    do {
      local_10 = -1;
      local_6 = 0;
      local_16 = local_4 * 10;
      local_8 = local_16 + 9;
      iVar1 = FUN_2388_0abf(uVar2,param_3,0,local_42);
      if (iVar1 == 0) {
        do {
          local_10 = local_10 + 1;
          if ((local_16 <= local_10) && (local_10 <= local_8)) {
            iVar1 = local_6 * 0xd;
            local_6 = local_6 + 1;
            FUN_2388_0626(iVar1 + 0x64f0,local_24);
          }
          iVar1 = FUN_2388_0ab4(0x2388,local_42);
        } while ((iVar1 == 0) && (local_6 < 10));
      }
      uVar2 = 0x2388;
      if (local_6 == 0) {
        local_e = 0;
      }
      else {
        iVar1 = 0;
        local_c = FUN_133d_32b2();
        local_a = iVar1;
        if (iVar1 == 0 && local_c == 0) break;
        if (local_4 != 0) {
          FUN_133d_0a0e(local_c,iVar1,0xd5);
        }
        for (local_12 = 0; local_12 < local_6; local_12 = local_12 + 1) {
          FUN_133d_0a0e(local_c,local_a,local_12 * 0xd + 0x64f0);
        }
        if (local_4 < *(int *)0x4e78 + -1) {
          FUN_133d_0a0e(local_c,local_a,0xdc);
        }
        local_e = 0;
        local_14 = FUN_133d_258e(local_c,local_a);
        local_14 = local_14 + -1;
        if (local_14 == 0x61) {
          local_4 = local_4 + -1;
LAB_1000_06f5:
          local_e = 1;
        }
        else {
          if (local_14 == 0x62) {
            local_4 = local_4 + 1;
            goto LAB_1000_06f5;
          }
          if (-1 < local_14) {
            *(int *)0x49ee = local_14 * 0xd + 0x64f0;
          }
        }
        uVar2 = 0x1cc9;
        FUN_1cc9_0310(local_c,local_a);
        local_a = 0;
        local_c = 0;
      }
    } while (local_e != 0);
  }
  if (local_a != 0 || local_c != 0) {
    FUN_1cc9_0310(local_c,local_a);
  }
  return local_14;
}



undefined2 __cdecl16far FUN_1000_0750(void)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  
  uVar1 = 0xffff;
  if (*(int *)0x50 != 0) {
    uVar1 = FUN_2388_0c4a(*(undefined2 *)0x5a,*(undefined2 *)0x5c,*(undefined2 *)0x4a8,
                          *(undefined2 *)0x4aa,12000);
    *(undefined2 *)0x52 = 1;
  }
  return uVar1;
}



undefined2 __cdecl16far FUN_1000_077e(void)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  
  uVar1 = 0xffff;
  if (*(int *)0x52 != 0) {
    uVar1 = FUN_2388_0c4a(*(undefined2 *)0x4a8,*(undefined2 *)0x4aa,*(undefined2 *)0x5a,
                          *(undefined2 *)0x5c,12000);
  }
  return uVar1;
}



void __cdecl16far FUN_1000_07a6(void)

{
  undefined2 *puVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x90 == 0) {
    FUN_1c5b_0004(0x22,0x96,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                  *(undefined2 *)0x3afa);
  }
  else {
    puVar1 = (undefined2 *)*(int *)0x90;
    FUN_1cb9_0000(*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                  *(undefined2 *)0x3afa,*puVar1,puVar1[1],puVar1[2],puVar1[3],0xf1,0x32,0x4f,0x96,0,
                  0);
  }
  FUN_1c86_000c(0,200,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                *(undefined2 *)0x3afa);
  return;
}



void __cdecl16far FUN_1000_082c(void)

{
  FUN_1c34_0044(0x96,0x4f,0x32);
  return;
}



void __cdecl16far FUN_1000_0842(int param_1,undefined2 param_2,int *param_3)

{
  undefined2 unaff_DS;
  undefined1 local_52 [80];
  
  FUN_2388_0626(local_52,0xe3);
  FUN_1334_006c(*(undefined2 *)(param_1 * 0x10 + 0x4ee6));
  FUN_2388_0e22(local_52);
  if ((7 < param_1) && (param_1 < 0x18)) {
    FUN_18a2_00b0(local_52);
    FUN_1334_006c(*(undefined2 *)0x4b5a);
    FUN_2388_0e22(local_52);
  }
  FUN_2388_05e6(local_52,0xe5);
  FUN_18ad_024e(local_52);
  *param_3 = *param_3 + *(byte *)*(undefined4 *)0x80 + 1;
  return;
}



void __cdecl16far FUN_1000_08e2(int param_1,undefined2 param_2,int *param_3)

{
  undefined2 unaff_DS;
  undefined1 local_52 [80];
  
  FUN_2388_0626(local_52,0xe7);
  FUN_1334_006c(*(undefined2 *)(param_1 * 2 + 0x4b5a));
  FUN_2388_0e22(local_52);
  FUN_2388_05e6(local_52,0xe9);
  FUN_18ad_024e(local_52);
  *param_3 = *param_3 + *(byte *)*(undefined4 *)0x80 + 1;
  return;
}



// WARNING: Removing unreachable block (ram,0x00010b4a)

void __cdecl16far FUN_1000_094e(void)

{
  byte bVar1;
  uint uVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  undefined2 uVar4;
  int local_58;
  undefined2 local_56;
  uint local_54;
  undefined1 local_52 [80];
  
  FUN_1000_07a6();
  local_58 = 0x33;
  local_56 = 0xf2;
  FUN_2388_0908(local_52,0xeb,*(undefined2 *)0x4b12,*(undefined2 *)0x4b14);
  FUN_18ad_024e(local_52);
  local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
  FUN_2388_0908(local_52,0xfa,*(undefined2 *)0x4b52,*(undefined2 *)0x4b54);
  FUN_18ad_024e(local_52);
  local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 6;
  FUN_18ad_024e(0x109);
  local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
  uVar2 = FUN_12ab_0112(*(undefined2 *)0x4b52,*(undefined2 *)0x4b54);
  local_54 = uVar2 & 0x1f;
  uVar3 = FUN_19b7_0006(uVar2 & 0xff,&local_56,&local_58);
  FUN_1000_0842(uVar3);
  if ((uVar2 & 0x40) != 0) {
    if ((uVar2 & 0x80) == 0) {
      uVar3 = 3;
    }
    else {
      uVar3 = 2;
    }
    FUN_1000_08e2(uVar3,&local_56,&local_58);
  }
  local_58 = local_58 + 10;
  FUN_18ad_024e(0x11c);
  local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
  if (*(char *)0x59 == '\0') {
    if (((*(char *)0x56 < '\b') || (*(char *)0x56 == '\x11')) || ('\x17' < *(char *)0x56)) {
      bVar1 = *(byte *)0x56;
    }
    else {
      bVar1 = *(byte *)0x56 & 7;
    }
    FUN_130b_0048(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba,(int)(char)bVar1,0x3af4,local_56,
                  local_58);
    if (*(byte *)0x56 == bVar1) goto LAB_1000_0b40;
    uVar3 = *(undefined2 *)0x4c2;
    uVar4 = *(undefined2 *)0x4c0;
  }
  else if ((*(byte *)0x58 & 0x40) == 0) {
    if ((*(byte *)0x58 & 0x20) == 0) goto LAB_1000_0b40;
    if ((*(byte *)0x58 & 0x80) == 0) {
      uVar3 = *(undefined2 *)0x4c2;
      uVar4 = *(undefined2 *)0x4c0;
    }
    else {
      uVar3 = *(undefined2 *)0x4c2;
      uVar4 = *(undefined2 *)0x4c0;
    }
  }
  else if ((*(byte *)0x58 & 0x80) == 0) {
    uVar3 = *(undefined2 *)0x4c2;
    uVar4 = *(undefined2 *)0x4c0;
  }
  else {
    uVar3 = *(undefined2 *)0x4c2;
    uVar4 = *(undefined2 *)0x4c0;
  }
  FUN_1d8f_0000(local_58,uVar4,uVar3);
LAB_1000_0b40:
  local_58 = local_58 + 0x14;
  if ((*(byte *)0x58 & 0x20) == 0) {
    if ((*(byte *)0x58 & 0x40) == 0) {
      FUN_1000_0842((int)*(char *)0x56,&local_56,&local_58);
      local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 5;
      FUN_18ad_024e(0x170);
      local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
      uVar3 = 0x183;
    }
    else {
      if ((*(byte *)0x58 & 0x80) == 0) {
        uVar3 = 3;
      }
      else {
        uVar3 = 2;
      }
      FUN_1000_08e2(uVar3,&local_56,&local_58);
      local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 5;
      FUN_18ad_024e(0x14b);
      local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
      uVar3 = 0x15c;
    }
  }
  else {
    if ((*(byte *)0x58 & 0x80) == 0) {
      uVar3 = 0x1c;
    }
    else {
      uVar3 = 0x1b;
    }
    FUN_1000_0842(uVar3,&local_56,&local_58);
    local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 5;
    FUN_18ad_024e(0x126);
    local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
    uVar3 = 0x137;
  }
  FUN_18ad_024e(uVar3);
  local_58 = local_58 + 0x14;
  FUN_2388_0908(local_52,0x198,*(undefined2 *)0x4a);
  FUN_18ad_024e(local_52);
  local_58 = local_58 + *(byte *)*(undefined4 *)0x80 + 1;
  if (*(int *)0x4e == 0) {
    uVar3 = 0x1ba;
  }
  else {
    uVar3 = 0x1a8;
  }
  FUN_18ad_024e(uVar3);
  FUN_1000_082c();
  return;
}



void __cdecl16far FUN_1000_0ce0(void)

{
  undefined2 unaff_DS;
  
  FUN_1c49_000e(*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                *(undefined2 *)0x3afa);
  FUN_1c34_0044(200,0x140,0);
  FUN_16d7_0944(*(undefined2 *)0x78,*(undefined2 *)0x7a,0,0,1);
  FUN_18a2_0068();
  FUN_1000_094e();
  return;
}



void __cdecl16far
FUN_1000_0d2a(undefined2 param_1,int param_2,undefined2 param_3,undefined2 param_4,int param_5)

{
  undefined2 unaff_DS;
  
  FUN_1d8f_0000(param_2,*(undefined2 *)0x4c0,*(undefined2 *)0x4c2);
  FUN_1ba3_0000(0x13,param_4,3,param_2 + 0xf);
  if (param_5 != 0) {
    *(undefined2 *)0x1ce = *(undefined2 *)0x716;
    FUN_1c86_000c(0xf,param_2 + 0x10,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                  *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  }
  return;
}



void __cdecl16far
FUN_1000_0d9e(undefined2 param_1,int param_2,undefined2 param_3,undefined2 param_4,int param_5)

{
  undefined2 unaff_DS;
  
  FUN_130b_0048(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba,param_3,0x3af4,param_1,param_2);
  FUN_1ba3_0000(0x13,param_4,3,param_2 + 0xf);
  if (param_5 != 0) {
    *(undefined2 *)0x1ce = *(undefined2 *)0x716;
    FUN_1c86_000c(0xf,param_2 + 0x10,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                  *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  }
  return;
}



void __cdecl16far FUN_1000_0e14(void)

{
  undefined2 unaff_DS;
  int iVar1;
  undefined2 uVar2;
  undefined2 local_12;
  int local_10;
  char local_e;
  int local_a;
  int local_8;
  int local_6;
  int local_4;
  
  FUN_1c49_000e(*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                *(undefined2 *)0x3afa);
  *(undefined2 *)0x716 = 0;
  for (local_4 = 0; local_4 < 2; local_4 = local_4 + 1) {
    for (local_a = 0; local_a < 8; local_a = local_a + 1) {
      local_6 = local_a * 0x11 + 1;
      local_8 = local_4 * 0x11 + 1;
      if (local_4 == 0) {
        local_e = '\0';
        local_10 = local_a;
      }
      else {
        local_e = '\b';
        local_10 = local_a;
        if (local_a == 1) {
          local_10 = 0x11;
        }
      }
      FUN_130b_0048(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba,local_10,0x3af4,local_6,local_8);
      if ((local_4 != 0) && (local_a != 1)) {
        FUN_1d8f_0000(local_8,*(undefined2 *)0x4c0,*(undefined2 *)0x4c2);
      }
      FUN_1ba3_0000(0x13,local_a,local_4,local_8 + 0xf);
      if (((char)(local_e + (char)local_a) == *(char *)0x56) && (*(char *)0x58 == '\0')) {
        FUN_1c86_000c(0xf,local_8 + 0x10,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                      *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
      }
    }
  }
  local_6 = 1;
  local_8 = 0x30;
  if ((*(char *)0x56 == '\x18') && (*(char *)0x58 == '\0')) {
    local_12 = 1;
  }
  else {
    local_12 = 0;
  }
  FUN_1000_0d9e(1,0x30,0x18,0,local_12);
  local_6 = local_6 + 0x11;
  FUN_1000_0d9e(local_6,local_8,0x19,1,*(char *)0x56 == '\x19');
  local_6 = local_6 + 0x11;
  FUN_1000_0d9e(local_6,local_8,0x1a,2,*(char *)0x56 == '\x1a');
  local_6 = local_6 + 0x11;
  FUN_1000_0d2a(local_6,local_8,4,3,*(char *)0x58 == -0x40);
  local_6 = local_6 + 0x11;
  FUN_1000_0d2a(local_6,local_8,0x14,4,*(char *)0x58 == '@');
  local_6 = local_6 + 0x11;
  FUN_1000_0d2a(local_6,local_8,0x24,5,*(char *)0x58 == -0x60);
  local_6 = local_6 + 0x11;
  FUN_1000_0d2a(local_6,local_8,0x34,6,*(char *)0x58 == ' ');
  local_6 = 0xa0;
  local_8 = 10;
  if ((*(byte *)0x58 & 0x20) == 0) {
    if ((*(byte *)0x58 & 0x40) != 0) {
      if ((*(byte *)0x58 & 0x80) == 0) {
        uVar2 = 3;
      }
      else {
        uVar2 = 2;
      }
      FUN_1000_08e2(uVar2,&local_6,&local_8);
      goto LAB_1000_10b5;
    }
    iVar1 = (int)*(char *)0x56;
  }
  else if ((*(byte *)0x58 & 0x80) == 0) {
    iVar1 = 0x1c;
  }
  else {
    iVar1 = 0x1b;
  }
  FUN_1000_0842(iVar1,&local_6,&local_8);
LAB_1000_10b5:
  FUN_1c34_0044(200,0x140,0);
  return;
}



void __cdecl16far FUN_1000_10cc(int param_1)

{
  undefined2 uVar1;
  int iVar2;
  undefined2 unaff_DS;
  char local_e;
  char local_c;
  char local_6;
  char local_4;
  
  if (param_1 == 0) {
    return;
  }
  uVar1 = *(undefined2 *)(param_1 * 0x10 + 0x5314);
  iVar2 = *(int *)(param_1 * 0x10 + 0x5312);
  if (iVar2 < 3) {
    local_c = (char)uVar1;
    local_6 = -1;
    local_4 = '\0';
    local_e = '\0';
    if (iVar2 != 0) {
      if (iVar2 == 1) {
        local_c = local_c + '\b';
      }
      else {
        local_c = local_c + '\x10';
      }
    }
    goto switchD_1000_1198_default;
  }
  switch(uVar1) {
  case 0:
    local_c = '\x18';
    local_6 = -1;
    goto LAB_1000_1126;
  case 1:
    local_c = '\x19';
    goto LAB_1000_1135;
  case 2:
    local_c = '\x1a';
LAB_1000_1135:
    local_6 = '@';
LAB_1000_1126:
    local_4 = '\0';
    local_e = '\0';
    goto switchD_1000_1198_default;
  case 3:
    local_6 = '\x1f';
    local_4 = -0x40;
    break;
  case 4:
    local_6 = '?';
    local_4 = '@';
    break;
  case 5:
    local_6 = '\x1f';
    local_4 = -0x60;
    break;
  case 6:
    local_6 = '_';
    local_4 = ' ';
    break;
  default:
    goto switchD_1000_1198_default;
  }
  local_c = '\0';
  local_e = '\x01';
switchD_1000_1198_default:
  if ((((local_c != *(char *)0x56) || (*(char *)0x57 != local_6)) || (*(char *)0x58 != local_4)) ||
     (*(char *)0x59 != local_e)) {
    *(char *)0x56 = local_c;
    *(char *)0x57 = local_6;
    *(char *)0x58 = local_4;
    *(char *)0x59 = local_e;
    FUN_1000_0e14();
  }
  return;
}



bool __cdecl16far FUN_1000_11ee(void)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x732 != 0) {
    uVar1 = FUN_1baa_0002();
    FUN_1000_10cc(uVar1);
  }
  return *(int *)0x730 == 0;
}



void __cdecl16far FUN_1000_1226(void)

{
  int iVar1;
  undefined2 unaff_DS;
  int local_6;
  int local_4;
  
  local_4 = 1;
  FUN_1000_0e14();
  FUN_1c21_002a();
  do {
    FUN_1c21_0042();
    iVar1 = FUN_1baf_0004();
    if (iVar1 != 0) {
      local_6 = 0;
      iVar1 = FUN_1baf_0018();
      if (iVar1 == 0x148) {
        local_6 = -8;
        if (*(int *)0x1ce == 8) {
          local_6 = 8;
        }
      }
      else if (iVar1 == 0x14b) {
        local_6 = -1;
      }
      else if (iVar1 == 0x14d) {
        local_6 = 1;
      }
      else if (iVar1 == 0x150) {
        local_6 = 8;
        if (*(int *)0x1ce == 0x10) {
          local_6 = -8;
        }
      }
      else {
        local_4 = 0;
      }
      if (local_6 != 0) {
        *(int *)0x1ce = *(int *)0x1ce + local_6 + -1;
        if (*(int *)0x1ce == 0x17) {
          *(undefined2 *)0x1ce = 0;
        }
        if (0x17 < *(int *)0x1ce) {
          *(int *)0x1ce = *(int *)0x1ce + -0x18;
        }
        if (*(int *)0x1ce < 0) {
          *(int *)0x1ce = *(int *)0x1ce + 8;
        }
        *(int *)0x1ce = *(int *)0x1ce + 1;
        FUN_1000_10cc(*(undefined2 *)0x1ce);
      }
    }
    if (*(int *)0x732 != 0) {
      local_4 = FUN_1000_11ee();
    }
    FUN_1c21_0110();
  } while (local_4 != 0);
  FUN_1000_0ce0();
  return;
}



void __cdecl16far FUN_1000_1310(void)

{
  int iVar1;
  undefined2 unaff_DS;
  
  FUN_1000_094e();
  *(uint *)0x6972 = (uint)(*(int *)0x6972 == 0);
  if ((((*(int *)0x49f2 <= *(int *)0x4b52) && (*(int *)0x4b52 < *(int *)0x49f2 + *(int *)0x52d4)) &&
      (*(int *)0x49f4 <= *(int *)0x4b54)) && (*(int *)0x4b54 < *(int *)0x49f4 + *(int *)0x52d8)) {
    iVar1 = ((*(int *)0x4b54 - *(int *)0x49f4) + *(int *)0x5af4) * *(int *)0x4e8c;
    FUN_1c67_0000(*(undefined2 *)0x4e8c,*(undefined2 *)0x4e8a,iVar1 + 8,*(undefined2 *)0x3af4,
                  *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa,
                  *(undefined2 *)0x3afc,*(undefined2 *)0x3afe,*(undefined2 *)0x3b00,
                  *(undefined2 *)0x3b02);
    if (*(int *)0x6972 != 0) {
      FUN_1d8f_0000(iVar1 + 8,*(undefined2 *)0x68,*(undefined2 *)0x6a);
    }
    FUN_1c34_0044(*(undefined2 *)0x4e8c,*(undefined2 *)0x4e8a,iVar1 + 8);
  }
  return;
}



void __cdecl16far FUN_1000_1404(undefined2 param_1,undefined2 param_2,int param_3)

{
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = FUN_12ab_000e(param_1,param_2);
  if (iVar1 != 0) {
    *(undefined2 *)0x4c8 = param_1;
    *(undefined2 *)0x4ca = param_2;
    if (*(int *)0x70 == 1) {
      if ((param_3 != 0) && (*(int *)0x6972 != 0)) {
        FUN_1000_1310();
      }
      *(undefined2 *)0x4b52 = param_1;
      *(undefined2 *)0x4b54 = param_2;
      if (param_3 != 0) {
        *(undefined2 *)0x6972 = 0;
        FUN_1000_1310();
      }
    }
    FUN_18a2_0068();
  }
  return;
}



int __cdecl16far FUN_1000_145e(int param_1,int param_2,int param_3,int param_4,undefined2 param_5)

{
  int iVar1;
  int iVar2;
  undefined2 unaff_DS;
  int local_4;
  
  local_4 = 0;
  iVar1 = param_3;
  if (param_3 < param_1) {
    iVar1 = param_1;
  }
  iVar2 = param_4;
  if (param_2 < param_4) {
    iVar2 = param_2;
  }
  if (param_4 < param_2) {
    param_4 = param_2;
  }
  if (param_1 < param_3) {
    param_3 = param_1;
  }
  if ((param_3 < *(int *)0x49f2 + 2) && (1 < *(int *)0x49f2)) {
    local_4 = 1;
  }
  if ((iVar2 < *(int *)0x49f4 + 2) && (1 < *(int *)0x49f4)) {
    local_4 = 1;
  }
  if ((*(int *)0x6026 + -2 < iVar1) && (*(int *)0x6026 < *(int *)0x4b12 + -2)) {
    local_4 = 1;
  }
  if ((*(int *)0x603e + -2 < param_4) && (*(int *)0x603e < *(int *)0x4b14 + -2)) {
    local_4 = 1;
  }
  if (local_4 != 0) {
    FUN_1000_1404(param_1,param_2,param_5);
  }
  return local_4;
}



void __cdecl16far FUN_1000_1514(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  bool bVar3;
  undefined2 unaff_DS;
  
  bVar3 = true;
  iVar1 = *(int *)0x4b52;
  iVar2 = *(int *)0x4b54;
  param_1 = iVar1 + param_1;
  param_2 = iVar2 + param_2;
  if ((param_1 < 1) || (param_2 < 1)) {
    bVar3 = false;
  }
  if ((*(int *)0x4b12 + -1 <= param_1) || (*(int *)0x4b14 + -1 <= param_2)) {
    bVar3 = false;
  }
  if (bVar3) {
    FUN_1000_145e(iVar1,iVar2,iVar1,iVar2,1);
    *(int *)0x4b52 = param_1;
    *(int *)0x4b54 = param_2;
    FUN_1000_1310();
  }
  return;
}



void __cdecl16far FUN_1000_157a(void)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x70 = 1;
  return;
}



void __cdecl16far FUN_1000_1582(int param_1)

{
  int iVar1;
  undefined2 unaff_DS;
  
  if (param_1 < 0) {
    param_1 = 0;
  }
  if (3 < param_1) {
    param_1 = 3;
  }
  *(int *)0x4d0 = param_1;
  FUN_1a47_0006();
  iVar1 = FUN_1000_145e(*(undefined2 *)0x4b52,*(undefined2 *)0x4b54,*(undefined2 *)0x4b52,
                        *(undefined2 *)0x4b54,0);
  if (iVar1 == 0) {
    FUN_18a2_0068();
  }
  return;
}



void __cdecl16far FUN_1000_15cc(void)

{
  undefined2 uVar1;
  undefined2 in_DX;
  int iVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  
  uVar1 = FUN_1d08_0082();
  *(undefined2 *)0x698a = uVar1;
  *(undefined2 *)0x698c = in_DX;
  uVar3 = (undefined2)((ulong)*(undefined4 *)0x78 >> 0x10);
  iVar2 = (int)*(undefined4 *)0x78;
  uVar1 = *(undefined2 *)(iVar2 + 0x4c);
  *(undefined2 *)0x698e = *(undefined2 *)(iVar2 + 0x4a);
  *(undefined2 *)0x6990 = uVar1;
  FUN_133d_36d8();
  return;
}



undefined2 __cdecl16far FUN_1000_15fc(void)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 local_56 [80];
  undefined2 local_6;
  undefined2 local_4;
  
  uVar2 = 1;
  iVar1 = FUN_133d_380a(0x14);
  if (iVar1 == 0) {
    FUN_2388_0626(local_56,0x4b64);
    FUN_1c04_005c(0x1f9,unaff_DS,local_56,unaff_SS,local_56,unaff_SS);
    local_6 = 0x3a;
    local_4 = 0x48;
    FUN_2388_0626(0x4a18,local_56);
    iVar1 = FUN_19f9_03ba(0x3a,0x48);
    if (iVar1 == 0) {
      *(undefined2 *)0x46 = 1;
      uVar2 = 0;
    }
  }
  return uVar2;
}



void __cdecl16far FUN_1000_1670(void)

{
  byte *pbVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  undefined2 uVar4;
  byte bVar5;
  undefined2 uVar6;
  uint uVar7;
  int iVar8;
  byte *pbVar9;
  int iVar10;
  undefined2 unaff_DS;
  undefined2 local_1c;
  undefined2 local_1a;
  undefined2 local_18;
  undefined2 local_16;
  undefined4 local_a;
  
  iVar8 = 0;
  uVar6 = FUN_1cc9_02e2();
  *(undefined2 *)0x8c = uVar6;
  *(int *)0x8e = iVar8;
  if (iVar8 != 0 || *(int *)0x8c != 0) {
    uVar7 = FUN_19c4_0002();
    FUN_1cc9_0310(*(undefined2 *)0x8c,*(undefined2 *)0x8e);
    *(undefined2 *)0x8e = 0;
    *(undefined2 *)0x8c = 0;
    if ((uVar7 & 1) != 0) {
      FUN_133d_36d8();
    }
    if ((uVar7 & 2) != 0) {
      FUN_133d_36d8();
    }
    uVar6 = *(undefined2 *)0x4b14;
    uVar3 = *(undefined2 *)0x4b0;
    uVar4 = *(undefined2 *)0x4b2;
    uVar2 = *(undefined2 *)0x4b12;
    FUN_1c3e_000a();
    FUN_1c4c_0000(*(undefined2 *)0x4b14,local_1c,local_1a,local_18,local_16,uVar6,uVar2,uVar3,uVar4)
    ;
    if (0 < *(int *)0x4b14) {
      iVar8 = 0;
      do {
        iVar10 = 0;
        if (0 < *(int *)0x4b12) {
          do {
            pbVar9 = (byte *)(*(int *)0x4b12 * iVar8 + local_18 + iVar10);
            local_a = (byte *)CONCAT22(local_16,pbVar9);
            pbVar1 = pbVar9;
            *pbVar1 = *pbVar1 & 0xf;
            bVar5 = FUN_12ab_0112(iVar10,iVar8);
            if (((bVar5 & 0x1f) == 0x19) ||
               (bVar5 = FUN_12ab_0112(iVar10,iVar8), (bVar5 & 0x1f) == 0x1a)) {
              *local_a = 0;
            }
            iVar10 = iVar10 + 1;
          } while (iVar10 < *(int *)0x4b12);
        }
        iVar8 = iVar8 + 1;
      } while (iVar8 < *(int *)0x4b14);
    }
    FUN_1c4c_0000(*(undefined2 *)0x4b14,*(undefined2 *)0x3b1c,*(undefined2 *)0x3b1e,
                  *(undefined2 *)0x3b20,*(undefined2 *)0x3b22,local_1c,local_1a,local_18,local_16);
    FUN_1000_0060();
    FUN_1c34_0044(200,0x140,0);
    FUN_1c46_0006();
    *(undefined2 *)0x46 = 1;
  }
  return;
}



void __cdecl16far FUN_1000_17e0(int param_1)

{
  int iVar1;
  undefined2 unaff_DS;
  bool bVar2;
  
  switch(param_1) {
  case 0x13:
    FUN_2388_0626(0x634e,0x4a18);
    FUN_2388_0a88(0x634e);
    iVar1 = FUN_133d_380a(0xe);
    if (iVar1 != 0) {
      return;
    }
    FUN_2388_0626(0x4a18,0x4b64);
    FUN_1c04_005c(0x24e,unaff_DS,0x4a18,unaff_DS,0x4a18,unaff_DS);
    iVar1 = FUN_19f9_02b0();
    goto joined_r0x000119c3;
  case 0x14:
    FUN_2388_0626(0x634e,0x4a18);
    FUN_2388_0a88(0x634e);
    bVar2 = *(int *)0x46 == 0;
    if (!bVar2) {
      iVar1 = FUN_133d_36d8();
      if (iVar1 == 1) {
        bVar2 = true;
      }
      else {
        bVar2 = false;
      }
    }
    if (bVar2) {
      FUN_1000_15fc();
    }
LAB_1000_196c:
    iVar1 = *(int *)0x4b12;
    *(int *)0x4c8 = iVar1 >> 1;
    *(int *)0x4b52 = iVar1 >> 1;
    iVar1 = *(int *)0x4b14;
    *(int *)0x4ca = iVar1 >> 1;
    *(int *)0x4b54 = iVar1 >> 1;
    FUN_1b56_04ca();
LAB_1000_1987:
    FUN_1000_0ce0();
    return;
  default:
    goto switchD_1000_17f7_caseD_15;
  case 0x1a:
    FUN_2388_0626(0x634e,0x4a18);
    FUN_2388_0a88(0x634e);
    iVar1 = FUN_133d_36d8();
    if (iVar1 != 1) {
      return;
    }
    iVar1 = FUN_19f9_02b0();
joined_r0x000119c3:
    if (iVar1 == 0) {
      *(undefined2 *)0x46 = 0;
      return;
    }
    break;
  case 0x1b:
    FUN_2388_0626(0x634e,0x4a18);
    FUN_2388_0a88(0x634e);
    bVar2 = *(int *)0x46 == 0;
    if (!bVar2) {
      iVar1 = FUN_133d_36d8();
      if (iVar1 == 1) {
        bVar2 = true;
      }
      else {
        bVar2 = false;
      }
    }
    if (!bVar2) {
      return;
    }
    iVar1 = FUN_1000_056a(0x28d,0x283,0x27e);
    if (iVar1 < 0) {
      return;
    }
    FUN_2388_0626(0x4a18,*(undefined2 *)0x49ee);
    iVar1 = FUN_19f9_0170();
    if (iVar1 == 0) {
      FUN_1000_00b6();
      goto LAB_1000_196c;
    }
    break;
  case 0x1f:
    FUN_2388_0626(0x634e,0x4a18);
    FUN_2388_0a88(0x634e);
    if (*(int *)0x46 == 0) {
LAB_1000_1a7e:
      *(undefined2 *)0x5e6e = 0;
      return;
    }
    iVar1 = FUN_133d_36d8();
    if (iVar1 != 2) {
      if (iVar1 != 1) {
        return;
      }
      goto LAB_1000_1a7e;
    }
    iVar1 = FUN_19f9_02b0();
    if (iVar1 == 0) goto LAB_1000_1a7e;
    break;
  case 0x21:
    FUN_1000_157a();
    return;
  case 0x24:
    iVar1 = *(int *)0x4d0 + -1;
    goto LAB_1000_1ac7;
  case 0x25:
    iVar1 = *(int *)0x4d0 + 1;
    goto LAB_1000_1ac7;
  case 0x26:
  case 0x27:
  case 0x28:
  case 0x29:
    iVar1 = -(param_1 + -0x29);
LAB_1000_1ac7:
    FUN_1000_1582(iVar1);
    return;
  case 0x2b:
    FUN_1000_1404(*(undefined2 *)0x4b52,*(undefined2 *)0x4b54,1);
    return;
  case 0x4a:
    FUN_1000_1670();
    return;
  case 0x4b:
    FUN_1000_1226();
    return;
  case 0x4c:
    *(int *)0x4a = (*(int *)0x4a + 1) % 3;
    goto LAB_1000_1b12;
  case 0x4d:
    *(uint *)0x4e = (uint)(*(int *)0x4e == 0);
LAB_1000_1b12:
    FUN_1000_094e();
    return;
  case 0x4e:
    if (*(int *)0x50 == 0) {
      return;
    }
    if (*(int *)0x52 != 0) {
      FUN_1000_077e();
    }
    goto LAB_1000_1987;
  case 0x51:
    break;
  case 0x52:
    break;
  case 0x53:
    break;
  case 0x54:
    break;
  case 0x5f:
    break;
  case 0x6a:
    FUN_1000_15cc();
    goto switchD_1000_17f7_caseD_15;
  }
  FUN_133d_36d8();
switchD_1000_17f7_caseD_15:
  return;
}



undefined2 __cdecl16far FUN_1000_1b84(void)

{
  int iVar1;
  int iVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  
  iVar1 = *(int *)0x724;
  uVar3 = 0;
  iVar2 = *(int *)0x726;
  if ((((0xfb < iVar1) && (8 < iVar2)) && (iVar1 < 0x134)) && (iVar2 < 0x30)) {
    uVar3 = 2;
  }
  if (((0xf0 < iVar1) && (0x31 < iVar2)) && ((iVar1 < 0x140 && (iVar2 < 200)))) {
    uVar3 = 3;
  }
  if (((-1 < iVar1) && (7 < iVar2)) && ((iVar1 < *(int *)0x64ec && (iVar2 < *(int *)0x64ee + 8)))) {
    uVar3 = 1;
  }
  return uVar3;
}



void __cdecl16far FUN_1000_1be0(int param_1,int param_2,int param_3)

{
  byte bVar1;
  byte *pbVar2;
  undefined2 in_DX;
  undefined2 unaff_DS;
  undefined4 local_e;
  byte local_4;
  byte local_3;
  
  if (((((0 < param_1) && (0 < param_2)) && (param_1 < *(int *)0x4b12 + -1)) &&
      ((param_2 < *(int *)0x4b14 + -1 && (*(int *)0x49f2 <= param_1)))) &&
     ((param_1 <= *(int *)0x6026 && ((*(int *)0x49f4 <= param_2 && (param_2 <= *(int *)0x603e))))))
  {
    pbVar2 = (byte *)FUN_12ab_00fa(param_1,param_2);
    local_e = (byte *)CONCAT22(in_DX,pbVar2);
    local_4 = *pbVar2;
    bVar1 = local_4 & 0x1f;
    local_3 = bVar1;
    if (((param_3 == 0) || (*(char *)0x59 != '\0')) &&
       ((((bVar1 != 0x19 && (bVar1 != 0x1a)) || (*(int *)0x4e == 0)) || (*(char *)0x56 < '\0')))) {
      if ((((bVar1 != 0x19) && (bVar1 != 0x1a)) || ((*(byte *)0x58 & 0x20) == 0)) &&
         (local_4 = local_4 & *(byte *)0x57, param_3 == 0)) {
        local_4 = local_4 | *(byte *)0x58;
      }
      if ((-1 < *(char *)0x56) && (param_3 == 0)) {
        local_3 = *(byte *)0x56;
      }
    }
    if ((param_3 != 0) && (*(char *)0x59 == '\0')) {
      if ((local_3 == 0x19) || (local_3 == 0x1a)) {
        *(byte *)0x56 = local_3;
        *(undefined1 *)0x57 = 0x40;
      }
      else {
        *(byte *)0x56 = local_3;
        *(undefined1 *)0x57 = 0xff;
      }
      *(undefined1 *)0x58 = 0;
      FUN_1000_094e();
      return;
    }
    if (*(char *)0x59 != '\0') {
      local_3 = bVar1;
    }
    local_3 = local_4 & 0xe0 | local_3;
    if (local_3 != *local_e) {
      *local_e = local_3;
      *(undefined2 *)0x46 = 1;
    }
  }
  return;
}



void __cdecl16far FUN_1000_1d28(int param_1,int param_2,undefined2 param_3)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined2 unaff_DS;
  
  param_1 = param_1 - *(int *)0x4a;
  param_2 = param_2 - *(int *)0x4a;
  iVar1 = *(int *)0x4a;
  iVar4 = iVar1 * 2 + 1;
  if (param_2 < param_2 + iVar4) {
    iVar2 = param_1;
    iVar5 = param_2;
    do {
      for (; iVar2 < param_1 + iVar4; iVar2 = iVar2 + 1) {
        iVar3 = FUN_12ab_000e(iVar2,iVar5);
        if (iVar3 != 0) {
          FUN_1000_1be0(iVar2,iVar5,param_3);
        }
      }
      iVar5 = iVar5 + 1;
      iVar2 = param_1;
    } while (iVar5 < param_2 + iVar4);
  }
  iVar1 = iVar1 * 2 + 3;
  FUN_18a2_000a(param_1 + -1,param_2 + -1,iVar1,iVar1);
  return;
}



undefined2 __cdecl16far FUN_1000_1db6(void)

{
  uint uVar1;
  char cVar2;
  int iVar3;
  undefined2 unaff_DS;
  undefined2 uVar4;
  undefined2 local_8;
  int local_6;
  int local_4;
  
  local_8 = 1;
  local_6 = 0;
  local_4 = 0;
  uVar1 = *(uint *)0x526c;
  if (uVar1 == 0x37) {
LAB_1000_1f48:
    local_6 = -1;
    local_4 = local_6;
    goto LAB_1000_1f58;
  }
  if ((int)uVar1 < 0x38) {
    if (uVar1 == 0x36) {
switchD_1000_1e69_caseD_14d:
      local_6 = 1;
      goto LAB_1000_1f58;
    }
    if (0x36 < uVar1) goto switchD_1000_1e69_caseD_14a;
    cVar2 = (char)uVar1;
    if (cVar2 == '\x1b') {
LAB_1000_1ec0:
      if (*(int *)0x46 != 0) {
        FUN_2388_0626(0x634e,0x4a18);
        FUN_2388_0a88(0x634e);
        iVar3 = FUN_133d_36d8();
        if (iVar3 == 2) {
          iVar3 = FUN_19f9_02b0();
          if (iVar3 != 0) {
            FUN_133d_36d8();
            goto LAB_1000_1f58;
          }
        }
        else if (iVar3 != 1) goto LAB_1000_1f58;
      }
      *(undefined2 *)0x5e6e = 0;
      goto LAB_1000_1f58;
    }
    if (cVar2 < '\x1c') {
      if (cVar2 == '\b') {
        if (*(char *)0x59 == '\0') {
          FUN_1000_1be0(*(undefined2 *)0x4b52,*(undefined2 *)0x4b54,1);
          goto LAB_1000_1f58;
        }
        FUN_1000_0750();
        uVar4 = 1;
LAB_1000_1eaa:
        FUN_1000_1d28(*(undefined2 *)0x4b52,*(undefined2 *)0x4b54,uVar4);
        goto LAB_1000_1f58;
      }
      if (cVar2 == '\r') goto LAB_1000_1eb8;
      if ((cVar2 == '\x11') || (cVar2 == '\x18')) goto LAB_1000_1ec0;
    }
    else {
      if (cVar2 == ' ') {
LAB_1000_1eb8:
        FUN_1000_0750();
        uVar4 = 0;
        goto LAB_1000_1eaa;
      }
      if (cVar2 == '1') goto switchD_1000_1e69_caseD_14f;
      if (cVar2 == '2') goto switchD_1000_1e69_caseD_150;
      if (cVar2 == '3') goto switchD_1000_1e69_caseD_151;
      if (cVar2 == '4') goto switchD_1000_1e69_caseD_14b;
    }
switchD_1000_1e69_caseD_14a:
    local_8 = 0;
    goto LAB_1000_1f58;
  }
  if (uVar1 != 0x148) {
    if (0x148 < (int)uVar1) {
      switch(uVar1) {
      case 0x149:
        goto switchD_1000_1e69_caseD_149;
      default:
        goto switchD_1000_1e69_caseD_14a;
      case 0x14b:
switchD_1000_1e69_caseD_14b:
        local_6 = -1;
        break;
      case 0x14d:
        goto switchD_1000_1e69_caseD_14d;
      case 0x14f:
switchD_1000_1e69_caseD_14f:
        local_6 = -1;
      case 0x150:
switchD_1000_1e69_caseD_150:
        local_4 = 1;
        break;
      case 0x151:
switchD_1000_1e69_caseD_151:
        local_6 = 1;
        local_4 = local_6;
      }
      goto LAB_1000_1f58;
    }
    if (uVar1 != 0x38) {
      if (uVar1 != 0x39) {
        if ((uVar1 == 0x110) || (uVar1 == 0x12d)) goto LAB_1000_1ec0;
        if (uVar1 == 0x147) goto LAB_1000_1f48;
        goto switchD_1000_1e69_caseD_14a;
      }
switchD_1000_1e69_caseD_149:
      local_6 = 1;
    }
  }
  local_4 = -1;
LAB_1000_1f58:
  if ((local_6 != 0) || (local_4 != 0)) {
    FUN_1000_1514(local_6,local_4);
    local_8 = 1;
  }
  return local_8;
}



bool __cdecl16far FUN_1000_1f7e(void)

{
  undefined2 unaff_DS;
  
  return *(int *)0x526c == 0xd;
}



undefined2 __cdecl16far FUN_1000_1f8e(void)

{
  int iVar1;
  int iVar2;
  undefined2 unaff_DS;
  int local_6;
  int local_4;
  
  local_4 = 0;
  if (*(int *)0x5ace == *(int *)0x4e8e) {
    iVar1 = FUN_1000_0186();
    if (*(int *)0x728 != 0) {
      FUN_1000_0750();
    }
    if (*(int *)0x732 != 0) {
      iVar2 = 0x10 >> (*(byte *)0x4d0 & 0x1f);
      local_4 = ((*(int *)0x726 + -8) / iVar2 - *(int *)0x5af4) + *(int *)0x49f4;
      local_6 = (*(int *)0x724 / iVar2 - *(int *)0x5ad8) + *(int *)0x49f2;
      iVar2 = FUN_12ab_000e(local_6,local_4);
      if (iVar2 == 0) {
        return 0;
      }
      if ((*(int *)0x4b52 != local_6) || (*(int *)0x4b54 != local_4)) {
        if (*(int *)0x6972 != 0) {
          FUN_1000_1310();
        }
        *(int *)0x4b52 = local_6;
        *(int *)0x4b54 = local_4;
      }
    }
    if (*(int *)0x732 != 0) {
      if (iVar1 == 0) {
        if ((*(int *)0x730 != 0) || (*(int *)0x720 != 0)) {
          FUN_1000_1404(local_6,local_4,1);
        }
      }
      else if ((*(int *)0x720 == 0) || (*(char *)0x59 != '\0')) {
        FUN_1000_1d28(local_6,local_4,*(undefined2 *)0x720);
      }
      else {
        FUN_1000_1be0(local_6,local_4,1);
      }
      if ((*(int *)0x730 != 0) && (*(int *)0x6972 == 0)) {
        FUN_1000_1310();
      }
    }
  }
  return 0;
}



void __cdecl16far FUN_1000_2082(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  if (*(int *)0x730 != 0) {
    iVar3 = *(int *)0x726;
    iVar1 = *(int *)0x49c2;
    iVar2 = FUN_1865_000e(*(int *)0x724 + *(int *)0x6982 + -0xfc,1,*(int *)0x4b12 + -2);
    iVar3 = FUN_1865_000e(iVar3 + iVar1 + -9,1,*(int *)0x4b14 + -2);
    if ((*(int *)0x4c8 != iVar2) || (*(int *)0x4ca != iVar3)) {
      FUN_1000_1404(iVar2,iVar3,1);
    }
  }
  return;
}



undefined2 __cdecl16far FUN_1000_20e0(void)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  
  uVar2 = FUN_1000_1b84();
  *(undefined2 *)0x5ace = uVar2;
  if (*(int *)0x728 != 0) {
    *(undefined2 *)0x4e8e = uVar2;
  }
  iVar1 = *(int *)0x4e8e;
  if (iVar1 == 1) {
    uVar2 = FUN_1000_1f8e();
    return uVar2;
  }
  if (iVar1 == 2) {
    FUN_1000_2082();
    return 0;
  }
  if (iVar1 != 3) {
    return 0;
  }
  if (*(int *)0x730 != 0) {
    FUN_1000_1226();
  }
  return 0;
}



void __cdecl16far FUN_1000_2124(void)

{
  int *piVar1;
  uint uVar2;
  undefined2 uVar3;
  int iVar4;
  int iVar5;
  undefined2 uVar6;
  uint uVar7;
  int in_DX;
  int iVar8;
  undefined2 unaff_DS;
  undefined4 uVar9;
  
  uVar2 = FUN_1d18_0006();
  in_DX = in_DX + (uint)(0xffeb < uVar2);
  *(int *)0x4ea0 = uVar2 + 0x14;
  *(int *)0x4ea2 = in_DX;
  uVar2 = 0;
  *(undefined2 *)0x6972 = 0;
  FUN_1000_1310();
  FUN_1c21_002a();
  do {
    uVar3 = FUN_1d18_0006();
    *(undefined2 *)0x4a14 = uVar3;
    *(int *)0x4a16 = in_DX;
    FUN_1c21_0042();
    iVar4 = FUN_1baf_0004();
    if ((iVar4 != 0) || (iVar5 = FUN_16d7_13b4(*(undefined2 *)0x78,*(undefined2 *)0x7a), iVar5 != 0)
       ) {
      if (*(int *)0x6972 == 0) {
        iVar5 = *(uint *)0x4a14 + 0x14;
        iVar8 = *(int *)0x4a16 + (uint)(0xffeb < *(uint *)0x4a14);
      }
      else {
        iVar5 = *(int *)0x4a14;
        iVar8 = *(int *)0x4a16;
      }
      *(int *)0x4ea0 = iVar5;
      *(int *)0x4ea2 = iVar8;
    }
    if ((*(int *)0x4ea2 <= *(int *)0x4a16) &&
       ((*(int *)0x4ea2 < *(int *)0x4a16 || (*(uint *)0x4ea0 <= *(uint *)0x4a14)))) {
      FUN_1000_1310();
      uVar7 = *(uint *)0x4a14;
      iVar5 = *(int *)0x4a16;
      *(int *)0x4ea0 = uVar7 + 0x14;
      *(int *)0x4ea2 = iVar5 + (uint)(0xffeb < uVar7);
    }
    if (iVar4 != 0) {
      iVar5 = FUN_1baf_0018();
      *(int *)0x526c = iVar5;
      if ((iVar5 < 0xff) && ((*(byte *)(iVar5 + 0x45a9) & 2) != 0)) {
        *(int *)0x526c = *(int *)0x526c + -0x20;
      }
      *(undefined2 *)0x4a68 = 0;
      uVar9 = FUN_16d7_1452(*(undefined2 *)0x78,*(undefined2 *)0x7a);
      *(int *)0x4a68 = (int)uVar9;
      if ((int)uVar9 == 0) {
        uVar9 = FUN_16d7_14e6(*(undefined2 *)0x78,*(undefined2 *)0x7a);
        *(undefined2 *)0x4a68 = (int)uVar9;
      }
      uVar3 = (undefined2)((ulong)uVar9 >> 0x10);
      if ((int)uVar9 == 0) {
        uVar6 = FUN_1000_1db6();
        uVar9 = CONCAT22(uVar3,uVar6);
        *(undefined2 *)0x4a68 = uVar6;
      }
      iVar5 = (int)((ulong)uVar9 >> 0x10);
      if ((int)uVar9 == 0) {
        uVar3 = FUN_1000_1f7e();
        *(undefined2 *)0x4a68 = uVar3;
      }
      uVar2 = FUN_1d18_0006();
      *(int *)0x4ea0 = uVar2 + 0x14;
      *(int *)0x4ea2 = iVar5 + (uint)(0xffeb < uVar2);
      uVar2 = 1;
    }
    if (iVar4 == 0) {
      piVar1 = (int *)*(undefined4 *)0x78;
      if ((*piVar1 == 0) &&
         (FUN_16d7_13b4((int *)piVar1,(int)((ulong)piVar1 >> 0x10)),
         *(int *)*(undefined4 *)0x78 == 0)) {
        uVar7 = FUN_1000_20e0();
        uVar2 = uVar2 | uVar7;
      }
    }
    if (*(int *)*(undefined4 *)0x78 != 0) {
      uVar2 = 1;
      FUN_1000_17e0(*(int *)*(undefined4 *)0x78);
    }
    in_DX = *(int *)0x5e6e;
    FUN_1c21_0110();
  } while (uVar2 == 0);
  return;
}



void __cdecl16far FUN_1000_229c(void)

{
  undefined2 unaff_DS;
  
  FUN_18a2_0068();
  do {
    FUN_1000_2124();
  } while (*(int *)0x5e6e != 0);
  return;
}



void __cdecl16far FUN_1000_22b0(void)

{
  undefined2 unaff_DS;
  
  FUN_1000_0196();
  FUN_16d7_0944(*(undefined2 *)0x78,*(undefined2 *)0x7a,0,0,1);
  FUN_1b56_00dc();
  FUN_18a2_0068();
  do {
    FUN_1000_094e();
    FUN_1000_229c();
  } while (*(int *)0x5e6e != 0);
  return;
}



void __cdecl16far FUN_1000_22e4(int param_1)

{
  undefined1 uVar1;
  undefined2 uVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  FUN_1842_0106();
  uVar2 = FUN_1842_01c8();
  iVar3 = param_1 * 0x10;
  *(undefined2 *)(iVar3 + 0x4ee6) = uVar2;
  uVar1 = FUN_1842_0198();
  *(undefined1 *)(iVar3 + 0x4ee8) = uVar1;
  uVar1 = FUN_1842_0198();
  *(undefined1 *)(iVar3 + 0x4ee9) = uVar1;
  uVar1 = FUN_1842_0198();
  *(undefined1 *)(iVar3 + 0x4eea) = uVar1;
  iVar3 = 0;
  do {
    uVar1 = FUN_1842_0198();
    *(undefined1 *)(param_1 * 0x10 + iVar3 + 0x4eed) = uVar1;
    iVar3 = iVar3 + 1;
  } while (iVar3 < 9);
  return;
}



void __cdecl16far FUN_1000_2336(void)

{
  undefined2 *puVar1;
  undefined1 *puVar2;
  undefined2 *puVar3;
  undefined1 uVar4;
  undefined2 uVar5;
  int iVar6;
  undefined2 *puVar7;
  undefined1 *puVar8;
  undefined2 *puVar9;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 local_1f [9];
  undefined2 local_16;
  undefined2 uStack_14;
  undefined2 uStack_12;
  undefined1 local_10 [10];
  int local_6;
  undefined2 *local_4;
  
  local_16 = *(undefined2 *)0x31f;
  uStack_14 = *(undefined2 *)0x321;
  uStack_12 = *(undefined2 *)0x323;
  puVar8 = local_10;
  for (iVar6 = 10; iVar6 != 0; iVar6 = iVar6 + -1) {
    puVar2 = puVar8;
    puVar8 = puVar8 + 1;
    *puVar2 = 0;
  }
  puVar8 = local_1f;
  for (iVar6 = 9; iVar6 != 0; iVar6 = iVar6 + -1) {
    puVar2 = puVar8;
    puVar8 = puVar8 + 1;
    *puVar2 = 0;
  }
  FUN_1842_001a(&local_16,0x32c);
  local_6 = 0;
  iVar6 = 0;
  do {
    FUN_1000_22e4(iVar6);
    iVar6 = iVar6 + 1;
  } while (iVar6 < 8);
  FUN_1842_001a(0,0x337);
  local_6 = 0;
  local_4 = (undefined2 *)0x4fe6;
  do {
    FUN_1000_22e4(local_6 + 8);
    puVar7 = local_4 + -0x40;
    puVar9 = local_4;
    for (iVar6 = 8; iVar6 != 0; iVar6 = iVar6 + -1) {
      puVar3 = puVar9;
      puVar9 = puVar9 + 1;
      puVar1 = puVar7;
      puVar7 = puVar7 + 1;
      *puVar3 = *puVar1;
    }
    local_6 = local_6 + 1;
    local_4 = local_4 + 8;
  } while (local_4 < (undefined2 *)0x5066);
  FUN_1842_001a(0,0x340);
  iVar6 = 0;
  do {
    FUN_1000_22e4(iVar6 + 0x18);
    iVar6 = iVar6 + 1;
  } while (iVar6 < 5);
  FUN_1842_001a(0,0x346);
  puVar7 = (undefined2 *)0x4b5a;
  do {
    FUN_1842_0106();
    uVar5 = FUN_1842_01c8();
    puVar9 = puVar7 + 1;
    *puVar7 = uVar5;
    puVar7 = puVar9;
  } while (puVar9 < (undefined2 *)0x4b64);
  iVar6 = FUN_1842_001a(0,0x352);
  if (iVar6 == 0) {
    FUN_1842_0106();
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x92 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x93 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x94 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x95 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x96 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x97 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x99 = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x9a = uVar4;
    uVar4 = FUN_1842_0198();
    *(undefined1 *)0x9b = uVar4;
  }
  FUN_1000_0018();
  return;
}



undefined2 __cdecl16far FUN_1000_247a(void)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  
  uVar2 = 1;
  if ((*(int *)0x4c == 0) && (*(int *)0x54 == 0)) {
    iVar1 = FUN_1000_056a(0x368,0x35e,0x359);
    if (iVar1 < 0) {
      *(undefined2 *)0x54 = 1;
    }
    else {
      FUN_2388_0626(0x4a18,*(undefined2 *)0x49ee);
    }
  }
  *(undefined2 *)0x4b12 = 0x3a;
  *(undefined2 *)0x4b14 = 0x48;
  *(undefined2 *)0x4d8 = 1;
  iVar1 = FUN_19f9_043e();
  if (iVar1 == 0) {
    if (*(int *)0x54 == 0) {
      iVar1 = FUN_19f9_0170();
      if (iVar1 != 0) {
        *(undefined2 *)0x62 = *(undefined2 *)0x4a4;
        return 1;
      }
      FUN_1000_00b6();
    }
    else {
      iVar1 = FUN_1000_15fc();
      if (iVar1 != 0) {
        return 1;
      }
    }
    iVar1 = *(int *)0x4b12;
    *(int *)0x4c8 = iVar1 >> 1;
    *(int *)0x4b52 = iVar1 >> 1;
    iVar1 = *(int *)0x4b14;
    *(int *)0x4ca = iVar1 >> 1;
    *(int *)0x4b54 = iVar1 >> 1;
    uVar2 = 0;
  }
  return uVar2;
}



void __cdecl16far FUN_1000_2516(void)

{
  byte bVar1;
  int iVar2;
  undefined2 uVar3;
  int iVar4;
  int extraout_DX;
  int extraout_DX_00;
  undefined2 unaff_DS;
  
  FUN_1334_000a(2000,0);
  FUN_1d1c_017d();
  FUN_1c3c_000a();
  FUN_1ec5_0008(0x13,1);
  FUN_1f65_0086(1,0x13);
  FUN_1f65_0007();
  FUN_1d75_0074();
  iVar2 = FUN_1ec0_000a(0xfc00,0xa000);
  if (iVar2 == 0) {
    FUN_1c3e_000a();
    if ((*(int *)0x3afa == 0 && *(int *)0x3af8 == 0) ||
       (FUN_1c3e_000a(), *(int *)0x3b02 == 0 && *(int *)0x3b00 == 0)) {
      *(undefined2 *)0x62 = 0x14;
    }
    else {
      FUN_1c34_000c();
      FUN_1c49_000e(*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                    *(undefined2 *)0x3afa);
      iVar2 = extraout_DX;
      FUN_1ec5_0024(0x3af4);
      uVar3 = FUN_1d43_000a();
      *(undefined2 *)0x3a96 = uVar3;
      *(int *)0x3a98 = iVar2;
      if (iVar2 == 0 && *(int *)0x3a96 == 0) {
        *(undefined2 *)0x62 = 0x15;
      }
      else {
        uVar3 = FUN_1d43_000a();
        *(undefined2 *)0x80 = uVar3;
        *(int *)0x82 = iVar2;
        if (iVar2 == 0 && *(int *)0x80 == 0) {
          *(undefined2 *)0x62 = 0x16;
        }
        else {
          FUN_133d_3092(*(undefined2 *)0x3a96,*(undefined2 *)0x3a98,0x800);
          FUN_1d70_000a(0xfc00,0xa000);
          iVar2 = extraout_DX_00;
          iVar4 = FUN_18f9_0bc2();
          if (iVar4 == 0) {
            uVar3 = FUN_1ddb_0008();
            *(undefined2 *)0x64 = uVar3;
            *(int *)0x66 = iVar2;
            if (iVar2 == 0 && *(int *)0x64 == 0) {
              *(undefined2 *)0x62 = 0x17;
            }
            else {
              uVar3 = FUN_1ddb_0008();
              *(undefined2 *)0x4c0 = uVar3;
              *(int *)0x4c2 = iVar2;
              *(undefined2 *)0x4c6 = 0;
              *(undefined2 *)0x4c4 = 0;
              if (iVar2 == 0 && *(int *)0x4c0 == 0) {
                *(undefined2 *)0x62 = 0x18;
              }
              else {
                *(undefined2 *)0x4c4 = *(undefined2 *)0x4c0;
                *(int *)0x4c6 = iVar2;
                uVar3 = FUN_1ddb_0008();
                *(undefined2 *)0x68 = uVar3;
                *(int *)0x6a = iVar2;
                if (iVar2 == 0 && *(int *)0x68 == 0) {
                  *(undefined2 *)0x62 = 0x19;
                }
                else {
                  iVar2 = 0x18;
                  FUN_1c3e_000a();
                  if (*(int *)0x4a00 == 0 && *(int *)0x49fe == 0) {
                    *(undefined2 *)0x62 = 0x1a;
                  }
                  else {
                    iVar4 = FUN_1ddb_0008();
                    if (iVar2 == 0 && iVar4 == 0) {
                      *(undefined2 *)0x62 = 0x1b;
                    }
                    else {
                      FUN_1d8f_0000(0,iVar4,iVar2);
                      FUN_1cc9_0310(iVar4,iVar2);
                      *(undefined2 *)0x56c = 0x49fa;
                      *(undefined2 *)0x632 = 0x49fa;
                      *(undefined2 *)0x90 = 0x49fa;
                      FUN_1b56_006c();
                      FUN_1000_2336();
                      bVar1 = *(byte *)0x97;
                      *(uint *)0x620 = (uint)bVar1;
                      *(uint *)0x61c = (uint)bVar1;
                      bVar1 = *(byte *)0x92;
                      *(uint *)0x62c = (uint)bVar1;
                      *(uint *)0x626 = (uint)bVar1;
                      bVar1 = *(byte *)0x94;
                      *(uint *)0x62e = (uint)bVar1;
                      *(uint *)0x628 = (uint)bVar1;
                      bVar1 = *(byte *)0x93;
                      *(uint *)0x630 = (uint)bVar1;
                      *(uint *)0x62a = (uint)bVar1;
                      FUN_19f9_0128();
                      iVar2 = 0;
                      uVar3 = FUN_1cc9_02e2();
                      *(undefined2 *)0x5a = uVar3;
                      *(int *)0x5c = iVar2;
                      if (iVar2 != 0 || *(int *)0x5a != 0) {
                        *(undefined2 *)0x50 = 1;
                      }
                      iVar2 = FUN_1000_247a();
                      if (iVar2 == 0) {
                        FUN_1d70_000a(0xfc00,0xa000);
                        *(undefined2 *)0x5e6e = 1;
                        FUN_1000_22b0();
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  else {
    *(undefined2 *)0x62 = 0x13;
  }
  FUN_1f65_004e();
  iVar2 = *(int *)0x49c6;
  FUN_1f65_0086(0,3);
  FUN_1ec5_0008(3,iVar2 != 3);
  FUN_1d1c_01f3();
  return;
}



void __cdecl16far FUN_1000_27de(void)

{
  FUN_2388_05a8(0x3aa);
  FUN_2388_05a8(0x3c3);
  return;
}



void __cdecl16far FUN_1000_27f6(void)

{
  FUN_1000_27de();
  FUN_2388_05a8(0x3ef);
  FUN_2388_05a8(0x3f9);
  FUN_2388_05a8(0x427);
  FUN_2388_05a8(0x458);
  return;
}



undefined2 __cdecl16far FUN_1000_2828(void)

{
  char cVar1;
  char in_AL;
  undefined2 uVar2;
  int *in_BX;
  undefined2 unaff_DS;
  
  uVar2 = 0;
  cVar1 = *(char *)*in_BX;
  while ((cVar1 != '\0' && (*(char *)*in_BX != in_AL))) {
    *in_BX = *in_BX + 1;
    cVar1 = *(char *)*in_BX;
  }
  if ((in_AL != '\0') && (*(char *)*in_BX == in_AL)) {
    *in_BX = *in_BX + 1;
    uVar2 = 1;
  }
  if (*(char *)*in_BX == '\0') {
    *in_BX = *in_BX + -1;
  }
  return uVar2;
}



void __cdecl16far FUN_1000_2872(undefined2 *param_1)

{
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = (int)*(char *)*param_1;
  if ((*(byte *)(iVar1 + 0x45a9) & 2) != 0) {
    iVar1 = iVar1 + -0x20;
  }
  if (iVar1 == 0x43) {
    *(undefined2 *)0x54 = 1;
    return;
  }
  if (iVar1 == 0x4d) {
    iVar1 = FUN_1000_2828();
    if (iVar1 != 0) {
      FUN_2388_0626(0x4a18,*param_1);
      FUN_1000_2828();
    }
    *(undefined2 *)0x4c = 1;
    return;
  }
  return;
}



void __cdecl16far FUN_1000_28d8(int param_1,int *param_2)

{
  char cVar1;
  int iVar2;
  int *piVar3;
  int iVar4;
  int iVar5;
  undefined2 unaff_DS;
  char *local_8;
  int local_6;
  int *local_4;
  
  *(undefined2 *)0x3c6a = 1;
  FUN_1f16_000e();
  FUN_1f3e_005a();
  *(undefined2 *)0x42 = 0;
  *(undefined2 *)0x44 = 0;
  iVar5 = 1;
  if (1 < param_1) {
    do {
      param_2 = param_2 + 1;
      iVar4 = FUN_2388_09e8(0x484,(int)*(char *)*param_2);
      iVar2 = local_6;
      piVar3 = local_4;
      if (iVar4 == 0) {
        if (*(char *)*param_2 == '?') {
          FUN_1000_27f6();
          goto LAB_1000_295c;
        }
      }
      else {
        local_8 = (char *)*param_2;
        cVar1 = *local_8;
        local_4 = param_2;
        local_6 = iVar5;
        while (cVar1 != '\0') {
          FUN_1000_2872(&local_8);
          local_8 = local_8 + 1;
          iVar2 = local_6;
          piVar3 = local_4;
          cVar1 = *local_8;
        }
      }
      local_4 = piVar3;
      local_6 = iVar2;
      iVar5 = iVar5 + 1;
    } while (iVar5 < param_1);
  }
  FUN_1000_2516();
LAB_1000_295c:
  FUN_1f3e_000c();
  if (*(int *)0x62 != 0) {
    FUN_2388_05a8(0x487,*(undefined2 *)0x62);
  }
  return;
}



undefined4 __stdcall16far FUN_1297_000c(undefined4 param_1)

{
  int in_AX;
  uint uVar1;
  int in_DX;
  int iVar2;
  int iVar3;
  undefined2 local_6;
  undefined1 local_4;
  
  iVar3 = 1;
  iVar2 = 0;
  if (0 < in_DX) {
    do {
      if (0 < iVar2) {
        iVar3 = iVar3 * 10;
      }
      FUN_2388_0e22((int)param_1,param_1._2_2_,0x498);
      iVar2 = iVar2 + 1;
    } while (iVar2 < in_DX);
  }
  iVar2 = FUN_2388_0dd4((int)param_1,param_1._2_2_);
  local_6 = iVar2 - in_DX;
  uVar1 = FUN_2388_0dd4((int)param_1,param_1._2_2_);
  if (local_6 < uVar1) {
    do {
      if (iVar3 <= in_AX) {
        iVar2 = in_AX / iVar3;
        in_AX = in_AX - iVar2 * iVar3;
        local_4 = (char)iVar2;
        *(char *)((int)param_1 + local_6) = *(char *)((int)param_1 + local_6) + local_4;
      }
      iVar3 = iVar3 / 10;
      uVar1 = FUN_2388_0dd4((int)param_1,param_1._2_2_);
      local_6 = local_6 + 1;
    } while (local_6 < uVar1);
  }
  return param_1;
}



undefined2 __cdecl16far
FUN_1297_00ba(undefined2 param_1,undefined2 param_2,char *param_3,undefined2 param_4)

{
  undefined2 unaff_DS;
  
  if (*param_3 == '*') {
    param_3 = param_3 + 1;
  }
  if (*(int *)0x49c == 0) {
    FUN_2388_0dec(param_1,param_2,param_3,param_4);
  }
  else {
    FUN_1c11_0004(param_3,param_4,0x4eaa,unaff_DS,param_1,param_2);
  }
  return param_1;
}



void __stdcall16far FUN_1297_0104(void)

{
  undefined1 local_52 [80];
  
  FUN_1297_00ba(local_52);
  FUN_2388_03a8(local_52);
  return;
}



void __cdecl16far FUN_1297_0142(void)

{
  FUN_1bfb_001c();
  return;
}



undefined2 __cdecl16far FUN_12ab_000e(int param_1,int param_2)

{
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 1;
  if ((((param_1 < 1) || (param_2 < 1)) || (*(int *)0x4b12 + -1 <= param_1)) ||
     (*(int *)0x4b14 + -1 <= param_2)) {
    local_4 = 0;
  }
  return local_4;
}



bool __cdecl16far FUN_12ab_0040(uint param_1,uint param_2,int param_3)

{
  bool bVar1;
  
  if ((int)param_1 < 1) {
    param_1 = ~param_1 + 1;
  }
  if ((int)param_2 < 1) {
    param_2 = ~param_2 + 1;
  }
  bVar1 = (int)(param_2 + param_1) < 2;
  if (param_3 != 1) {
    if (((int)param_1 < 2) && ((int)param_2 < 2)) {
      bVar1 = true;
    }
    if (((param_3 != 2) && (bVar1 = (bool)(bVar1 | (int)(param_2 + param_1) < 3), param_3 != 3)) &&
       (((int)param_1 < 2 || ((int)param_2 < 2)))) {
      bVar1 = true;
    }
  }
  return bVar1;
}



undefined2 __cdecl16far FUN_12ab_00c4(int param_1,int param_2)

{
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 1;
  if ((param_1 < *(int *)0x49f2) || (*(int *)0x6026 < param_1)) {
    local_4 = 0;
  }
  if ((param_2 < *(int *)0x49f4) || (*(int *)0x603e < param_2)) {
    local_4 = 0;
  }
  return local_4;
}



int __cdecl16far FUN_12ab_00fa(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return param_2 * *(int *)0x4b12 + *(int *)0x4a8 + param_1;
}



undefined2 __cdecl16far FUN_12ab_0112(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return CONCAT11((char)((uint)(*(int *)0x4b12 * param_2) >> 8),
                  *(undefined1 *)(*(int *)0x4b12 * param_2 + *(int *)0x4a8 + param_1));
}



int __cdecl16far FUN_12ab_012e(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return *(int *)0x4b12 * param_2 + *(int *)0x4ac + param_1;
}



undefined2 __cdecl16far FUN_12ab_0146(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return CONCAT11((char)((uint)(*(int *)0x4b12 * param_2) >> 8),
                  *(undefined1 *)(*(int *)0x4b12 * param_2 + *(int *)0x4ac + param_1));
}



void __cdecl16far FUN_12ab_0162(undefined2 param_1,undefined2 param_2,byte param_3,int param_4)

{
  byte *pbVar1;
  undefined2 in_DX;
  undefined4 local_6;
  
  pbVar1 = (byte *)FUN_12ab_012e(param_1,param_2);
  local_6 = (byte *)CONCAT22(in_DX,pbVar1);
  if (param_4 != 0) {
    *local_6 = *local_6 | param_3;
    return;
  }
  *local_6 = *local_6 & ~param_3;
  return;
}



int __cdecl16far FUN_12ab_0198(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return *(int *)0x4b12 * param_2 + *(int *)0x4b0 + param_1;
}



undefined2 __cdecl16far FUN_12ab_01b0(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return CONCAT11((char)((uint)(param_2 * *(int *)0x4b12) >> 8),
                  *(undefined1 *)(param_2 * *(int *)0x4b12 + *(int *)0x4b0 + param_1));
}



byte __cdecl16far FUN_12ab_01ce(undefined2 param_1,undefined2 param_2)

{
  byte bVar1;
  
  bVar1 = FUN_12ab_01b0(param_1,param_2);
  return bVar1 & 0xf;
}



void __cdecl16far FUN_12ab_01e0(undefined2 param_1,undefined2 param_2,byte param_3)

{
  byte *pbVar1;
  undefined2 in_DX;
  undefined4 local_6;
  
  pbVar1 = (byte *)FUN_12ab_0198(param_1,param_2);
  local_6 = (byte *)CONCAT22(in_DX,pbVar1);
  *local_6 = *local_6 ^ (*local_6 ^ param_3) & 0xf;
  return;
}



uint __cdecl16far FUN_12ab_0204(undefined2 param_1,undefined2 param_2)

{
  uint uVar1;
  undefined2 local_4;
  
  uVar1 = FUN_12ab_01b0(param_1,param_2);
  local_4 = uVar1 >> 4 & 0xf;
  if (local_4 == 0xf) {
    local_4 = 0xffff;
  }
  return local_4 & 0xff;
}



void __cdecl16far FUN_12ab_022c(int param_1,int param_2,int param_3)

{
  int iVar1;
  byte *pbVar2;
  undefined2 in_DX;
  undefined2 extraout_DX;
  undefined4 local_6;
  
  if (param_3 < 4) {
    iVar1 = FUN_12ab_0380(param_1,param_2);
    if (-1 < iVar1) {
      FUN_1000_000c(0x518,param_3,param_1,param_2);
      FUN_1000_000e(5);
      FUN_1ed0_03d6(param_2,param_2 >> 0xf,param_1,param_1 >> 0xf);
      in_DX = extraout_DX;
    }
  }
  pbVar2 = (byte *)FUN_12ab_0198(param_1,param_2);
  local_6 = (byte *)CONCAT22(in_DX,pbVar2);
  *local_6 = *local_6 & 0xf | (char)param_3 << 4;
  return;
}



uint __cdecl16far FUN_12ab_02a4(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  undefined2 local_4;
  
  local_4 = 0xffff;
  iVar1 = FUN_12ab_000e(param_1,param_2);
  if (iVar1 != 0) {
    iVar1 = FUN_19b7_006c(param_1,param_2);
    if (iVar1 == 0) {
      local_4 = FUN_12ab_01ce(param_1,param_2);
      local_4 = local_4 & 0xff;
    }
  }
  return local_4;
}



int __cdecl16far FUN_12ab_02e4(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return *(int *)0x4b12 * param_2 + *(int *)0x4b4 + param_1;
}



undefined2 __cdecl16far FUN_12ab_02fc(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  return CONCAT11((char)((uint)(*(int *)0x4b12 * param_2) >> 8),
                  *(undefined1 *)(*(int *)0x4b12 * param_2 + *(int *)0x4b4 + param_1));
}



int __cdecl16far FUN_12ab_0318(undefined2 param_1,undefined2 param_2)

{
  char cVar1;
  uint uVar2;
  undefined2 local_4;
  
  local_4 = -1;
  uVar2 = FUN_12ab_0146(param_1,param_2);
  if ((uVar2 & 1) != 0) {
    cVar1 = FUN_12ab_0204(param_1,param_2);
    local_4 = (int)cVar1;
  }
  return local_4;
}



int __cdecl16far FUN_12ab_0346(undefined2 param_1,undefined2 param_2)

{
  char cVar1;
  uint uVar2;
  undefined2 local_4;
  
  local_4 = -1;
  uVar2 = FUN_12ab_0146(param_1,param_2);
  if ((uVar2 & 2) != 0) {
    cVar1 = FUN_12ab_0204(param_1,param_2);
    local_4 = (int)cVar1;
    if (3 < local_4) {
      local_4 = -1;
    }
  }
  return local_4;
}



int __cdecl16far FUN_12ab_0380(undefined2 param_1,undefined2 param_2)

{
  char cVar1;
  uint uVar2;
  undefined2 local_4;
  
  local_4 = -1;
  uVar2 = FUN_12ab_0146(param_1,param_2);
  if ((uVar2 & 2) != 0) {
    cVar1 = FUN_12ab_0204(param_1,param_2);
    local_4 = (int)cVar1;
    if (local_4 < 4) {
      local_4 = -1;
    }
  }
  return local_4;
}



int __cdecl16far FUN_12ab_03ba(undefined2 param_1,undefined2 param_2)

{
  char cVar1;
  uint uVar2;
  undefined2 local_4;
  
  local_4 = -1;
  uVar2 = FUN_12ab_0146(param_1,param_2);
  if ((uVar2 & 2) != 0) {
    cVar1 = FUN_12ab_0204(param_1,param_2);
    local_4 = (int)cVar1;
  }
  return local_4;
}



void __cdecl16far FUN_12ab_03e8(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  
  iVar1 = FUN_12ab_03ba(param_1,param_2);
  if (iVar1 < 0) {
    FUN_12ab_0318(param_1,param_2);
  }
  return;
}



int __cdecl16far FUN_12ab_040a(undefined2 param_1,undefined2 param_2,int param_3)

{
  char cVar1;
  uint uVar2;
  int iVar3;
  undefined2 unaff_DS;
  undefined2 local_6;
  
  local_6 = -1;
  uVar2 = FUN_12ab_0146(param_1,param_2);
  if ((uVar2 & 0x48) != 0) {
    cVar1 = FUN_12ab_0204(param_1,param_2);
    iVar3 = (int)cVar1;
    if ((((-1 < iVar3) && (iVar3 < 4)) && (iVar3 != param_3)) &&
       ((*(byte *)(param_3 * 0x13c + iVar3 + 0x4eb4) & 0x40) != 0)) {
      local_6 = iVar3;
    }
  }
  return local_6;
}



int __cdecl16far FUN_12ab_0458(uint param_1,uint param_2)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  undefined2 unaff_DS;
  uint local_a;
  int local_8;
  char local_4;
  
  local_8 = -1;
  if ((*(int *)0x4dc != 0) && (iVar1 = FUN_12ab_0380(param_1,param_2), iVar1 < 0)) {
    local_a = FUN_12ab_0112(param_1,param_2);
    local_a = local_a & 0x3f;
    if (((local_a < 8) || (0xf < local_a)) && ((local_a < 0x10 || (0x17 < local_a)))) {
      local_4 = '\0';
    }
    else {
      local_4 = '\x01';
    }
    uVar2 = (param_1 & 3) * 4 + (param_2 & 3);
    uVar3 = (byte)((((char)((int)param_2 >> 2) * '\x03' + (char)((int)param_1 >> 2)) - local_4) +
                  *(char *)0x4dc) & 0xf;
    if ((uVar3 == uVar2) || ((byte)((byte)uVar3 ^ 10) == uVar2)) {
      iVar1 = FUN_19b7_0032(param_1,param_2);
      local_8 = *(int *)(iVar1 * 2 + 0x4de);
      if (local_8 == 0) {
        local_8 = 6;
      }
      uVar2 = FUN_12ab_0146(param_1,param_2);
      if ((uVar2 & 4) != 0) {
        if (local_8 == 0xc) {
          return 0;
        }
        local_8 = -1;
      }
    }
  }
  return local_8;
}



undefined2 __cdecl16far FUN_12ab_0540(uint param_1,uint param_2)

{
  char cVar1;
  int iVar2;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  if (*(int *)0x4dc != 0) {
    iVar2 = FUN_19b7_0032(param_1,param_2);
    if (((iVar2 != 0x19) && (iVar2 != 0x1a)) && (iVar2 != 0x18)) {
      cVar1 = FUN_12ab_0204(param_1,param_2);
      if ((cVar1 < '\0') &&
         ((((int)param_2 >> 2) * 0x13 + ((int)param_1 >> 2) * 0x11 + *(int *)0x4dc + 8U & 0x1f) +
          (param_1 & 3) * -4 == (param_2 & 3))) {
        local_4 = 1;
      }
    }
  }
  return local_4;
}



uint __cdecl16far FUN_12ab_05bc(byte param_1)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  uVar1 = (uint)param_1;
  uVar2 = uVar1 & 0x1f;
  param_1 = (byte)uVar2;
  if (*(int *)0x4da == 2) {
    iVar3 = 0;
    if ((uVar2 < 0x18) && (7 < uVar2)) {
      return (uint)(param_1 & 7 | 8);
    }
  }
  else {
    iVar3 = *(int *)0x4da + -3;
    if ((iVar3 == 0) && (uVar2 < 0x18)) {
      return uVar1 & 7;
    }
  }
  return CONCAT11((char)((uint)iVar3 >> 8),param_1);
}



void __cdecl16far FUN_130b_0006(void)

{
  FUN_1cc9_02e2();
  return;
}



int __cdecl16far FUN_130b_001e(int param_1)

{
  if ((param_1 != 0x11) && (param_1 != 9)) {
    if (7 < param_1) {
      param_1 = param_1 + -0xf;
    }
    return param_1;
  }
  return 8;
}



void __cdecl16far
FUN_130b_0048(int param_1,undefined2 param_2,undefined2 param_3,int param_4,undefined2 param_5,
             undefined2 param_6)

{
  undefined1 *puVar1;
  undefined1 *puVar2;
  int iVar3;
  int iVar4;
  undefined1 *puVar5;
  undefined1 *puVar6;
  int iVar7;
  undefined2 unaff_DS;
  
  iVar4 = FUN_130b_001e(param_3);
  puVar5 = (undefined1 *)FUN_1c91_0000();
  iVar3 = *(int *)(param_4 + 2);
  puVar6 = (undefined1 *)(iVar4 * 0x100 + param_1);
  iVar4 = 0x10;
  do {
    for (iVar7 = 0x10; iVar7 != 0; iVar7 = iVar7 + -1) {
      puVar2 = puVar5;
      puVar5 = puVar5 + 1;
      puVar1 = puVar6;
      puVar6 = puVar6 + 1;
      *puVar2 = *puVar1;
    }
    puVar5 = puVar5 + iVar3 + -0x10;
    iVar4 = iVar4 + -1;
  } while (iVar4 != 0);
  return;
}



void __cdecl16far
FUN_130b_00ac(int param_1,undefined2 param_2,undefined2 param_3,int param_4,undefined2 param_5,
             undefined2 param_6)

{
  int iVar1;
  int iVar2;
  char *pcVar3;
  char *pcVar4;
  int iVar5;
  char *pcVar6;
  char *pcVar7;
  undefined2 unaff_DS;
  
  iVar2 = FUN_130b_001e(param_3);
  pcVar3 = (char *)FUN_1c91_0000();
  iVar1 = *(int *)(param_4 + 2);
  pcVar4 = (char *)(iVar2 * 0x100 + param_1);
  iVar2 = 0x10;
  do {
    iVar5 = 0x10;
    do {
      while (pcVar7 = pcVar3, pcVar6 = pcVar4, *pcVar7 != '\0') {
        iVar5 = iVar5 + -1;
        pcVar4 = pcVar6 + 1;
        pcVar3 = pcVar7 + 1;
        if (iVar5 == 0) goto LAB_130b_0114;
      }
      *pcVar7 = *pcVar6;
      iVar5 = iVar5 + -1;
      pcVar4 = pcVar6 + 1;
      pcVar3 = pcVar7 + 1;
    } while (iVar5 != 0);
LAB_130b_0114:
    pcVar4 = pcVar6 + 1;
    pcVar3 = pcVar7 + iVar1 + -0xf;
    iVar2 = iVar2 + -1;
    if (iVar2 == 0) {
      return;
    }
  } while( true );
}



void __cdecl16far
FUN_130b_011e(int param_1,undefined2 param_2,undefined2 param_3,int param_4,undefined2 param_5,
             int param_6,byte param_7)

{
  undefined1 *puVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  undefined1 *puVar5;
  int iVar6;
  int iVar7;
  undefined1 *puVar8;
  undefined1 *puVar9;
  undefined2 unaff_DS;
  
  iVar3 = FUN_130b_001e(param_3);
  iVar4 = 0x10 >> (param_7 & 0x1f);
  iVar6 = 1 << (param_7 & 0x1f);
  param_6 = param_6 - (iVar4 + -1);
  puVar5 = (undefined1 *)FUN_1c91_0000();
  iVar2 = *(int *)(param_4 + 2);
  puVar8 = (undefined1 *)(iVar3 * 0x100 + param_1 + (iVar6 >> 1) * 0x11);
  iVar3 = iVar4;
  puVar9 = puVar8;
  iVar7 = iVar4;
  do {
    do {
      puVar1 = puVar5;
      puVar5 = puVar5 + 1;
      *puVar1 = *puVar8;
      puVar8 = puVar8 + iVar6;
      iVar3 = iVar3 + -1;
    } while (iVar3 != 0);
    puVar8 = puVar9 + (0x10 << (param_7 & 0x1f));
    puVar5 = puVar5 + (iVar2 - iVar4);
    iVar7 = iVar7 + -1;
    iVar3 = iVar4;
    puVar9 = puVar8;
  } while (iVar7 != 0);
  return;
}



void __cdecl16far
FUN_130b_01d4(int param_1,undefined2 param_2,undefined2 param_3,int param_4,undefined2 param_5,
             int param_6,byte param_7)

{
  char *pcVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  char *pcVar6;
  int iVar7;
  int iVar8;
  char *pcVar9;
  char *pcVar10;
  undefined2 unaff_DS;
  
  iVar4 = FUN_130b_001e(param_3);
  iVar5 = 0x10 >> (param_7 & 0x1f);
  iVar7 = 1 << (param_7 & 0x1f);
  param_6 = param_6 - (iVar5 + -1);
  pcVar6 = (char *)FUN_1c91_0000();
  iVar2 = *(int *)(param_4 + 2);
  pcVar9 = (char *)(iVar4 * 0x100 + param_1 + (iVar7 >> 1) * 0x11);
  iVar4 = iVar5;
  pcVar10 = pcVar9;
  iVar8 = iVar5;
  do {
    do {
      if (*pcVar6 == '\0') {
        pcVar1 = pcVar6;
        pcVar6 = pcVar6 + 1;
        *pcVar1 = *pcVar9;
        iVar3 = iVar7;
      }
      else {
        iVar3 = iVar7 + 1;
      }
      iVar4 = iVar4 + -1;
      pcVar9 = pcVar9 + iVar3;
    } while (iVar4 != 0);
    pcVar9 = pcVar10 + (0x10 << (param_7 & 0x1f));
    pcVar6 = pcVar6 + (iVar2 - iVar5);
    iVar8 = iVar8 + -1;
    iVar4 = iVar5;
    pcVar10 = pcVar9;
  } while (iVar8 != 0);
  return;
}



void __cdecl16far FUN_1334_000a(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  FUN_1f45_0000(0x534,unaff_DS,param_1,param_2,0x4a02,unaff_DS);
  *(undefined2 *)0x6040 = 0;
  return;
}



undefined2 __cdecl16far FUN_1334_0036(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  undefined4 uVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  
  uVar2 = 0x4a02;
  uVar3 = unaff_DS;
  FUN_2388_0dd4(param_1,param_2,0x4a02);
  uVar1 = FUN_1f45_010a(uVar2,uVar3);
  FUN_2388_0dec(uVar1,param_1,param_2);
  uVar3 = *(undefined2 *)0x6040;
  *(int *)0x6040 = *(int *)0x6040 + 1;
  return uVar3;
}



char * __cdecl16far FUN_1334_006c(int param_1)

{
  char *pcVar1;
  int iVar2;
  char *pcVar3;
  undefined2 unaff_DS;
  bool bVar4;
  
  pcVar3 = (char *)*(int *)0x4a04;
  if (param_1 != 0) {
    bVar4 = true;
    do {
      iVar2 = -1;
      do {
        if (iVar2 == 0) break;
        iVar2 = iVar2 + -1;
        pcVar1 = pcVar3;
        pcVar3 = pcVar3 + 1;
        bVar4 = *pcVar1 == '\0';
      } while (!bVar4);
      if (!bVar4) {
        return pcVar3;
      }
      param_1 = param_1 + -1;
      bVar4 = param_1 == 0;
    } while (!bVar4);
  }
  return pcVar3;
}



void FUN_133d_000e(undefined4 param_1)

{
  int iVar1;
  undefined4 uVar2;
  undefined2 uVar3;
  
  iVar1 = (int)param_1 + 0x84;
  uVar3 = param_1._2_2_;
  FUN_2388_0684();
  uVar2 = FUN_1f45_010a(iVar1,uVar3);
  uVar3 = (undefined2)((ulong)uVar2 >> 0x10);
  *(undefined2 *)((int)param_1 + 0x6c) = (int)uVar2;
  *(undefined2 *)((int)param_1 + 0x6e) = uVar3;
  FUN_2388_0dec(*(undefined2 *)((int)param_1 + 0x6c),uVar3);
  return;
}



void FUN_133d_0050(undefined2 param_1,undefined2 param_2)

{
  uint uVar1;
  undefined2 uVar2;
  int in_DX;
  undefined2 unaff_DS;
  undefined1 local_16 [3];
  char local_13;
  char local_11;
  
  if (*(int *)0x55c < 8) {
    uVar2 = FUN_1000_0012(*(undefined2 *)0x55c,*(undefined2 *)0x5e44);
    uVar2 = FUN_1000_0010(uVar2);
    FUN_2388_0626(local_16,0x577,uVar2);
    local_13 = local_13 + *(char *)0x55c;
    local_11 = local_11 + (char)uVar2;
  }
  else {
    FUN_2388_0626(local_16,0x572);
    *(undefined2 *)0x56e = 1;
    *(undefined2 *)0x4e7e = 1;
    uVar1 = FUN_1d18_0006();
    *(int *)0x4e86 = uVar1 + 0xf0;
    *(int *)0x4e88 = in_DX + (uint)(0xff0f < uVar1);
  }
  FUN_133d_000e(param_1,param_2);
  return;
}



void FUN_133d_00d0(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  undefined1 local_16 [3];
  char local_13;
  
  FUN_2388_0626(local_16,0x57e);
  local_13 = local_13 + *(char *)0x55e;
  FUN_133d_000e(param_1,param_2);
  return;
}



void FUN_133d_00fa(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  undefined1 local_16 [3];
  char local_13;
  
  FUN_2388_0626(local_16,0x583);
  local_13 = local_13 + *(char *)0x560;
  FUN_133d_000e(param_1,param_2);
  return;
}



void FUN_133d_0124(undefined4 param_1)

{
  undefined2 uVar1;
  int iVar2;
  bool bVar3;
  undefined2 uVar4;
  undefined2 uVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  undefined2 uVar10;
  undefined2 unaff_DS;
  undefined4 uVar11;
  undefined4 local_8c;
  undefined1 local_82 [80];
  undefined1 local_32 [48];
  
  uVar10 = (undefined2)((ulong)param_1 >> 0x10);
  iVar8 = (int)param_1;
  if (*(int *)(iVar8 + 0x6e) != 0 || *(int *)(iVar8 + 0x6c) != 0) {
    bVar3 = 7 < *(int *)0x55c;
    uVar11 = FUN_1f45_010a(iVar8 + 0x84,uVar10);
    iVar6 = (int)((ulong)uVar11 >> 0x10);
    *(undefined2 *)(iVar8 + 0x68) = (int)uVar11;
    *(int *)(iVar8 + 0x6a) = iVar6;
    iVar2 = *(int *)(iVar8 + 0x68);
    *(undefined2 *)(iVar2 + 0x12) = 0;
    *(undefined2 *)(iVar2 + 0x10) = 0;
    iVar7 = iVar6;
    if (bVar3) {
      local_8c = FUN_1f45_010a(iVar8 + 0x84,uVar10);
      iVar7 = (int)((ulong)local_8c >> 0x10);
      *(undefined2 *)(iVar2 + 0x10) = (int)local_8c;
      *(int *)(iVar2 + 0x12) = iVar7;
    }
    FUN_2388_0dec(local_82);
    uVar1 = *(undefined2 *)0x3c64;
    *(undefined2 *)0x3c64 = 0;
    FUN_1000_0016(1);
    FUN_2388_0c4a(local_32);
    *(undefined2 *)0x3ae0 = 0xfc00;
    *(undefined2 *)0x3ae2 = 0xa000;
    uVar4 = FUN_1ddb_0008();
    *(undefined2 *)(iVar2 + 0xc) = uVar4;
    *(int *)(iVar2 + 0xe) = iVar7;
    if (iVar7 == 0 && *(int *)(iVar2 + 0xc) == 0) {
      switchD_1000:bb3f::caseD_1f();
      *(undefined2 *)0x570 = 1;
      uVar4 = FUN_1000_000a();
      *(undefined2 *)(iVar2 + 0xc) = uVar4;
      *(int *)(iVar2 + 0xe) = iVar7;
    }
    FUN_2388_0c4a(0xfc00,0xa000,local_32);
    *(undefined2 *)0x3ae2 = 0;
    *(undefined2 *)0x3ae0 = 0;
    if (*(int *)(iVar2 + 0xe) == 0 && *(int *)(iVar2 + 0xc) == 0) {
      *(undefined2 *)(iVar8 + 0x6a) = 0;
      *(undefined2 *)(iVar8 + 0x68) = 0;
    }
    else {
      iVar9 = (int)local_8c;
      uVar4 = (undefined2)((ulong)local_8c >> 0x10);
      if (bVar3) {
        FUN_2388_05e6(local_82,0x588);
        if (*(int *)0x570 == 0) {
          uVar5 = FUN_1ddb_0008();
          *(undefined2 *)(iVar9 + 0xc) = uVar5;
          *(int *)(iVar9 + 0xe) = iVar7;
        }
        if ((*(int *)(iVar9 + 0xe) == 0 && *(int *)(iVar9 + 0xc) == 0) && (*(int *)0x570 == 0)) {
          *(undefined2 *)0x570 = 2;
          switchD_1000:bb3f::caseD_1f();
        }
        if (*(int *)0x570 != 0) {
          uVar5 = FUN_1000_000a();
          *(undefined2 *)(iVar9 + 0xc) = uVar5;
          *(int *)(iVar9 + 0xe) = iVar7;
        }
      }
      FUN_1000_0016(0);
      if ((bVar3) && (*(int *)(iVar9 + 0xe) == 0 && *(int *)(iVar9 + 0xc) == 0)) {
        if (*(int *)0x570 != 1) {
          FUN_1cc9_0310(*(undefined2 *)(iVar2 + 0xc),*(undefined2 *)(iVar2 + 0xe));
        }
        *(undefined2 *)(iVar8 + 0x6a) = 0;
        *(undefined2 *)(iVar8 + 0x68) = 0;
      }
      FUN_1d70_000a(0xfc00,0xa000);
    }
    *(undefined2 *)0x3ae2 = 0;
    *(undefined2 *)0x3ae0 = 0;
    *(undefined2 *)0x3c64 = uVar1;
  }
  return;
}



void FUN_133d_034a(void)

{
  undefined2 *puVar1;
  undefined2 unaff_DS;
  char in_stack_00000008;
  undefined2 in_stack_00000010;
  undefined2 in_stack_00000012;
  undefined2 in_stack_00000014;
  undefined2 in_stack_00000016;
  undefined2 in_stack_00000018;
  
  if ((*(int *)0x56c != 0) && (in_stack_00000008 == '\a')) {
    if (*(int *)0x58a != 0) {
      FUN_1c4c_0000(in_stack_00000010,in_stack_00000012,in_stack_00000014,in_stack_00000016,
                    in_stack_00000018,*(undefined2 *)0x3afc,*(undefined2 *)0x3afe,
                    *(undefined2 *)0x3b00,*(undefined2 *)0x3b02);
      return;
    }
    puVar1 = (undefined2 *)*(int *)0x56c;
    FUN_1cb9_0000(in_stack_00000012,in_stack_00000014,in_stack_00000016,in_stack_00000018,*puVar1,
                  puVar1[1],puVar1[2],puVar1[3]);
    return;
  }
  FUN_1c5b_0004(in_stack_00000008,in_stack_00000010,in_stack_00000012,in_stack_00000014,
                in_stack_00000016,in_stack_00000018);
  return;
}



void __cdecl16far FUN_133d_03de(int param_1)

{
  FUN_2388_0dec(param_1 * 0x40 + 0x634e);
  return;
}



void __cdecl16far FUN_133d_03fa(undefined2 param_1,undefined2 param_2)

{
  undefined2 uVar1;
  undefined2 in_DX;
  
  uVar1 = FUN_1334_006c(param_2);
  FUN_133d_03de(param_1,uVar1,in_DX);
  return;
}



void __cdecl16far FUN_133d_043a(int param_1,undefined2 param_2,undefined2 param_3)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)(param_1 * 4 + 0x698a) = param_2;
  *(undefined2 *)(param_1 * 4 + 0x698c) = param_3;
  return;
}



void __cdecl16far
FUN_133d_0454(undefined2 *param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5,undefined2 param_6,undefined2 param_7,undefined2 param_8,
             undefined2 param_9)

{
  undefined2 *puVar1;
  undefined2 uVar2;
  
  uVar2 = (undefined2)((ulong)param_1 >> 0x10);
  puVar1 = (undefined2 *)param_1;
  *param_1 = param_4;
  puVar1[1] = param_5;
  puVar1[2] = param_6;
  puVar1[3] = param_7;
  puVar1[4] = param_8;
  puVar1[5] = param_9;
  puVar1[6] = param_2;
  puVar1[7] = param_3;
  return;
}



void FUN_133d_0494(undefined2 param_1,undefined2 param_2,undefined4 param_3)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  char *local_54;
  char local_52 [80];
  
  FUN_2388_0dec(local_52);
  local_54 = local_52;
  while (*local_54 != '\0') {
    iVar1 = FUN_2388_09e8(0x58c,*local_54);
    if (iVar1 == 0) {
      local_54 = local_54 + 1;
    }
    else {
      FUN_2388_0626(local_54,local_54 + 1);
    }
  }
  uVar2 = (undefined2)((ulong)param_3 >> 0x10);
  FUN_1d6c_0002(local_52,unaff_SS,*(undefined2 *)((int)param_3 + 0xc),
                *(undefined2 *)((int)param_3 + 0xe));
  return;
}



void FUN_133d_0504(undefined4 param_1)

{
  int in_AX;
  int in_DX;
  int iVar1;
  undefined2 uVar2;
  
  iVar1 = (int)param_1;
  uVar2 = (undefined2)((ulong)param_1 >> 0x10);
  if (in_AX == 0) {
    if (in_DX == 0) {
      uVar2 = *(undefined2 *)(iVar1 + 10);
    }
    else {
      uVar2 = *(undefined2 *)(iVar1 + 10);
    }
  }
  else {
    uVar2 = *(undefined2 *)(iVar1 + 10);
  }
  FUN_1d6a_0006(uVar2);
  return;
}



int FUN_133d_0546(char *param_1,undefined2 param_2,int *param_3)

{
  char cVar1;
  int in_AX;
  undefined2 uVar2;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int local_10;
  undefined4 local_a;
  char local_4 [2];
  
  local_4[1] = 0;
  FUN_133d_0504((int *)param_3,param_3._2_2_);
  local_a = (char *)CONCAT22(param_2,param_1);
  local_10 = in_AX;
  do {
    if (*local_a == '\0') {
      return local_10;
    }
    cVar1 = *local_a;
    if (cVar1 == '{') {
      uVar2 = 1;
LAB_133d_063e:
      *(undefined2 *)0x562 = uVar2;
LAB_133d_0621:
      FUN_133d_0504((int *)param_3,param_3._2_2_);
    }
    else {
      if (cVar1 == '|') {
        return local_10;
      }
      if (cVar1 == '}') {
        uVar2 = 0;
        goto LAB_133d_063e;
      }
      if (cVar1 == '~') {
        FUN_133d_0504((int *)param_3,param_3._2_2_);
        local_a = (char *)CONCAT22(local_a._2_2_,(char *)local_a + 1);
        local_4[0] = *local_a;
        local_10 = FUN_1d53_0008(*param_3,local_4,unaff_SS,((int *)param_3)[6],((int *)param_3)[7]);
        local_10 = local_10 + *param_3;
        goto LAB_133d_0621;
      }
      local_4[0] = *local_a;
      local_10 = FUN_1d53_0008(*param_3,local_4,unaff_SS,((int *)param_3)[6],((int *)param_3)[7]);
      local_10 = local_10 + *param_3;
    }
    local_a = (char *)CONCAT22(local_a._2_2_,(char *)local_a + 1);
  } while( true );
}



uint FUN_133d_0650(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  int iVar2;
  int in_DX;
  int iVar3;
  undefined2 unaff_DS;
  uint local_4;
  
  local_4 = 0;
  iVar1 = FUN_2388_0d82(param_1,param_2,0x7e);
  iVar3 = in_DX;
  iVar2 = FUN_2388_0ca8(param_1,param_2,0x7e);
  if ((((iVar2 == iVar1) && (iVar3 == in_DX)) || (*(char *)(iVar2 + 1) != 'F')) ||
     ((*(byte *)(*(byte *)(iVar1 + 1) + 0x45a9) & 4) == 0)) {
    if (in_DX != 0 || iVar1 != 0) {
      if ((*(byte *)(*(byte *)(iVar1 + 1) + 0x45a9) & 2) == 0) {
        local_4 = (uint)*(byte *)(iVar1 + 1);
      }
      else {
        local_4 = *(byte *)(iVar1 + 1) - 0x20;
      }
    }
  }
  else {
    local_4 = *(byte *)(iVar1 + 1) + 0x10a;
  }
  return local_4;
}



undefined2 * __cdecl16far FUN_133d_06de(int param_1,undefined2 param_2,undefined2 param_3)

{
  byte bVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  undefined2 uVar4;
  undefined2 uVar5;
  undefined2 uVar6;
  undefined2 *puVar7;
  int iVar8;
  undefined2 unaff_DS;
  undefined2 *local_16;
  int local_14;
  undefined4 local_a;
  
  local_14 = 0;
  local_16 = (undefined2 *)0x0;
  *(undefined2 *)0x56e = 0;
  *(undefined2 *)0x570 = 0;
  iVar8 = 0;
  puVar7 = (undefined2 *)FUN_1cc9_02e2();
  if (iVar8 != 0 || puVar7 != (undefined2 *)0x0) {
    local_a = (undefined2 *)CONCAT22(iVar8,puVar7);
    FUN_1f45_0090(param_1,param_1 >> 0xf,puVar7 + 0x4b,iVar8,puVar7 + 0x42,iVar8);
    puVar7[0x27] = 0;
    puVar7[0x26] = 0;
    puVar7[0x29] = 0;
    puVar7[0x28] = 0;
    puVar7[5] = *(undefined2 *)0x556;
    puVar7[6] = *(undefined2 *)0x558;
    puVar7[7] = *(undefined2 *)0x55a;
    *(undefined2 *)0x558 = 0xffff;
    *(undefined2 *)0x55a = 0xffff;
    puVar7[0x14] = 0x50;
    puVar7[0x11] = 4;
    puVar7[0x19] = 4;
    puVar7[0x1e] = *(undefined2 *)0x53c;
    puVar7[0x1f] = *(undefined2 *)0x53e;
    puVar7[0x20] = *(undefined2 *)0x540;
    puVar7[0x21] = *(undefined2 *)0x542;
    puVar7[0x22] = *(undefined2 *)0x544;
    bVar1 = *(byte *)(puVar7 + 5);
    puVar7[0x23] = -(uint)((bVar1 & 0x10) == 0) & 3;
    puVar7[0x24] = -(uint)((bVar1 & 0x10) == 0) & 2;
    puVar7[0x2b] = 0;
    puVar7[0x2a] = 0;
    puVar7[0x2d] = 0;
    puVar7[0x2c] = 0;
    puVar7[0x2f] = 0;
    puVar7[0x2e] = 0;
    puVar7[0x31] = 0;
    puVar7[0x30] = 0;
    puVar7[0x39] = 0;
    puVar7[0x38] = 0;
    puVar7[0x33] = 0;
    puVar7[0x32] = 0;
    puVar7[0x35] = 0;
    puVar7[0x34] = 0;
    puVar7[0x37] = 0;
    puVar7[0x36] = 0;
    uVar2 = *(undefined2 *)0x552;
    uVar3 = *(undefined2 *)0x550;
    uVar4 = *(undefined2 *)0x54e;
    uVar5 = *(undefined2 *)0x54c;
    uVar6 = *(undefined2 *)0x54a;
    *local_a = 0;
    puVar7[1] = 0;
    puVar7[2] = 0;
    puVar7[3] = 0;
    puVar7[4] = 0;
    *(undefined2 *)0x556 = 0;
    puVar7[0x10] = 0;
    puVar7[0x12] = 0;
    puVar7[0x13] = 0;
    puVar7[0x15] = 0;
    puVar7[0x16] = 0;
    puVar7[0x17] = 0;
    puVar7[0x18] = 0;
    puVar7[0x1a] = 0;
    puVar7[0x1b] = 0;
    puVar7[0x1c] = 0;
    puVar7[0x25] = 0;
    FUN_133d_0454(puVar7 + 0x3a,iVar8,param_2,param_3,0,uVar6,uVar5,uVar4,uVar3,uVar2);
    local_16 = puVar7;
    local_14 = iVar8;
    if (*(int *)0x49c0 == 0) {
      *(byte *)(puVar7 + 5) = *(byte *)(puVar7 + 5) | 0x80;
    }
  }
  if ((iVar8 != 0 || puVar7 != (undefined2 *)0x0) && ((puVar7 != local_16 || (iVar8 != local_14))))
  {
    FUN_1cc9_0310(puVar7,iVar8);
  }
  return local_16;
}



undefined4 __stdcall16far FUN_133d_08a8(undefined4 param_1)

{
  int *piVar1;
  bool bVar2;
  int in_AX;
  int iVar3;
  int iVar4;
  undefined2 uVar5;
  int local_c;
  int local_a;
  
  bVar2 = false;
  local_a = 0;
  local_c = 0;
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  iVar4 = *(int *)((int)param_1 + 0x56);
  iVar3 = *(int *)((int)param_1 + 0x54);
  while ((!bVar2 && (iVar4 != 0 || iVar3 != 0))) {
    if (*(int *)(iVar3 + 4) == in_AX) {
      bVar2 = true;
      local_c = iVar3;
      local_a = iVar4;
    }
    else {
      piVar1 = (int *)(iVar3 + 0x10);
      iVar4 = *(int *)(iVar3 + 0x12);
      iVar3 = *piVar1;
    }
  }
  return CONCAT22(local_a,local_c);
}



void __cdecl16far
FUN_133d_0908(undefined2 param_1,undefined2 param_2,undefined2 param_3,int param_4)

{
  undefined4 local_6;
  
  local_6 = (byte *)FUN_133d_08a8(param_1,param_2);
  if (param_4 != 0) {
    *local_6 = *local_6 | 1;
    return;
  }
  *local_6 = *local_6 & 0xfe;
  return;
}



void __cdecl16far
FUN_133d_0938(undefined2 param_1,undefined2 param_2,undefined2 param_3,int param_4)

{
  undefined4 local_6;
  
  local_6 = (byte *)FUN_133d_08a8(param_1,param_2);
  if (param_4 != 0) {
    *local_6 = *local_6 | 2;
    return;
  }
  *local_6 = *local_6 & 0xfd;
  return;
}



void __cdecl16far FUN_133d_0968(undefined4 param_1)

{
  byte *pbVar1;
  byte *pbVar2;
  int iVar3;
  undefined2 uVar4;
  undefined4 local_6;
  
  uVar4 = (undefined2)((ulong)param_1 >> 0x10);
  iVar3 = *(int *)((int)param_1 + 0x56);
  pbVar2 = (byte *)*(undefined2 *)((int)param_1 + 0x54);
  while( true ) {
    local_6 = (byte *)CONCAT22(iVar3,pbVar2);
    if (iVar3 == 0 && pbVar2 == (byte *)0x0) break;
    *local_6 = *local_6 & 0xfe;
    pbVar1 = pbVar2 + 0x10;
    iVar3 = *(int *)(pbVar2 + 0x12);
    pbVar2 = *(byte **)pbVar1;
  }
  return;
}



undefined2 __cdecl16far FUN_133d_0998(undefined2 param_1,undefined2 param_2)

{
  long lVar1;
  undefined2 local_4;
  
  local_4 = 0;
  lVar1 = FUN_133d_08a8(param_1,param_2);
  if (lVar1 != 0) {
    local_4 = *(undefined2 *)((int)lVar1 + 6);
  }
  return local_4;
}



void __cdecl16far
FUN_133d_09c8(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  long lVar1;
  
  lVar1 = FUN_133d_08a8(param_1,param_2);
  if (lVar1 != 0) {
    *(undefined2 *)((int)lVar1 + 6) = param_4;
  }
  return;
}



void __cdecl16far FUN_133d_09f0(undefined4 param_1)

{
  undefined4 uVar1;
  
  uVar1 = FUN_133d_08a8((int)param_1,param_1._2_2_);
  *(undefined2 *)((int)param_1 + 0x4c) = (int)uVar1;
  *(undefined2 *)((int)param_1 + 0x4e) = (int)((ulong)uVar1 >> 0x10);
  return;
}



byte * __cdecl16far FUN_133d_0a0e(undefined4 param_1,char *param_2,undefined2 param_3)

{
  int iVar1;
  byte *pbVar2;
  int iVar3;
  undefined2 uVar4;
  int iVar5;
  undefined2 uVar6;
  int iVar7;
  int iVar8;
  undefined2 uVar9;
  byte *pbVar10;
  undefined4 uVar11;
  int local_1e;
  int iStack_1c;
  undefined1 local_18 [20];
  int local_4;
  
  uVar9 = (undefined2)((ulong)param_1 >> 0x10);
  iVar8 = (int)param_1;
  local_1e = 0;
  iStack_1c = 0;
  iVar3 = *(int *)(iVar8 + 0x56);
  iVar7 = *(int *)(iVar8 + 0x54);
  while (iVar1 = iVar7, iVar5 = iVar3, iVar5 != 0 || iVar1 != 0) {
    iVar7 = *(int *)(iVar1 + 0x10);
    local_1e = iVar1;
    iStack_1c = iVar5;
    iVar3 = *(int *)(iVar1 + 0x12);
  }
  pbVar10 = (byte *)FUN_1f45_010a(iVar8 + 0x84,uVar9);
  uVar6 = (undefined2)((ulong)pbVar10 >> 0x10);
  pbVar2 = (byte *)pbVar10;
  if (iStack_1c == 0 && local_1e == 0) {
    *(undefined2 *)(iVar8 + 0x54) = pbVar2;
    *(undefined2 *)(iVar8 + 0x56) = uVar6;
    *(undefined2 *)(iVar8 + 0x4c) = pbVar2;
    *(undefined2 *)(iVar8 + 0x4e) = uVar6;
    (pbVar2 + 0x16)[0] = 0;
    (pbVar2 + 0x16)[1] = 0;
    (pbVar2 + 0x14)[0] = 0;
    (pbVar2 + 0x14)[1] = 0;
  }
  else {
    *(undefined2 *)(local_1e + 0x10) = pbVar2;
    *(undefined2 *)(local_1e + 0x12) = uVar6;
    *(int *)(pbVar2 + 0x14) = local_1e;
    *(int *)(pbVar2 + 0x16) = iStack_1c;
  }
  *(undefined2 *)(iVar8 + 0x70) = pbVar2;
  *(undefined2 *)(iVar8 + 0x72) = uVar6;
  if ((*(byte *)(iVar8 + 10) & 4) == 0) {
    local_18[0] = 0;
  }
  else {
    FUN_2388_0626(local_18,0x591);
  }
  (pbVar2 + 0x12)[0] = 0;
  (pbVar2 + 0x12)[1] = 0;
  (pbVar2 + 0x10)[0] = 0;
  (pbVar2 + 0x10)[1] = 0;
  pbVar10[0] = 0;
  pbVar10[1] = 0;
  (pbVar2 + 6)[0] = 0;
  (pbVar2 + 6)[1] = 0;
  iVar3 = iVar8 + 0x84;
  uVar4 = uVar9;
  FUN_2388_0dd4((char *)param_2,param_2._2_2_,iVar3,uVar9);
  uVar11 = FUN_1f45_010a(iVar3,uVar4);
  *(int *)(pbVar2 + 8) = (int)uVar11;
  *(int *)(pbVar2 + 10) = (int)((ulong)uVar11 >> 0x10);
  FUN_2388_0dec(*(undefined2 *)(pbVar2 + 8),*(undefined2 *)(pbVar2 + 10),local_18);
  FUN_2388_0e22(*(undefined2 *)(pbVar2 + 8),*(undefined2 *)(pbVar2 + 10),(char *)param_2,
                param_2._2_2_);
  if (*param_2 == '\0') {
    *pbVar10 = *pbVar10 | 1;
  }
  *(undefined2 *)(pbVar2 + 4) = param_3;
  uVar4 = FUN_133d_0650((char *)param_2,param_2._2_2_);
  *(undefined2 *)(pbVar2 + 2) = uVar4;
  uVar11 = FUN_133d_0494(*(undefined2 *)(pbVar2 + 8),*(undefined2 *)(pbVar2 + 10),iVar8 + 0x74,uVar9
                        );
  iVar7 = (int)((ulong)uVar11 >> 0x10);
  local_4 = (int)uVar11 + *(int *)(iVar8 + 0x48) * 2 + *(int *)(iVar8 + 0x22);
  iVar3 = FUN_2388_0ca8(*(undefined2 *)(pbVar2 + 8),*(undefined2 *)(pbVar2 + 10),0x7c);
  if (iVar7 != 0 || iVar3 != 0) {
    local_4 = local_4 + *(int *)(iVar8 + 0x22);
  }
  iVar3 = *(int *)(iVar8 + 0x20);
  if (*(int *)(iVar8 + 0x20) < local_4) {
    iVar3 = local_4;
  }
  *(int *)(iVar8 + 0x20) = iVar3;
  *(int *)(iVar8 + 2) = *(int *)(iVar8 + 2) + 1;
  return pbVar2;
}



int __cdecl16far
FUN_133d_0bf6(undefined4 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5)

{
  byte *pbVar1;
  int iVar2;
  int in_DX;
  undefined2 uVar3;
  
  uVar3 = (undefined2)((ulong)param_1 >> 0x10);
  pbVar1 = (byte *)((int)param_1 + 10);
  *pbVar1 = *pbVar1 | 5;
  iVar2 = FUN_133d_0a0e((int)param_1,uVar3,param_2,param_3,param_4);
  if (in_DX != 0 || iVar2 != 0) {
    *(undefined2 *)(iVar2 + 6) = param_5;
  }
  return iVar2;
}



void __cdecl16far FUN_133d_0c30(undefined4 param_1,undefined2 param_2)

{
  *(undefined2 *)((int)param_1 + 0x28) = param_2;
  return;
}



byte * __cdecl16far FUN_133d_0c40(undefined4 param_1,char *param_2)

{
  int iVar1;
  char *pcVar2;
  int iVar3;
  byte *pbVar4;
  int iVar5;
  int iVar6;
  undefined2 uVar7;
  int iVar8;
  undefined2 uVar9;
  undefined2 uVar10;
  byte *pbVar11;
  undefined4 uVar12;
  int local_6;
  int iStack_4;
  
  uVar9 = (undefined2)((ulong)param_1 >> 0x10);
  iVar8 = (int)param_1;
  local_6 = 0;
  iStack_4 = 0;
  iVar5 = *(int *)(iVar8 + 0x5a);
  iVar3 = *(int *)(iVar8 + 0x58);
  while (iVar1 = iVar3, iVar6 = iVar5, iVar6 != 0 || iVar1 != 0) {
    iVar3 = *(int *)(iVar1 + 6);
    local_6 = iVar1;
    iStack_4 = iVar6;
    iVar5 = *(int *)(iVar1 + 8);
  }
  pbVar11 = (byte *)FUN_1f45_010a(iVar8 + 0x84,uVar9);
  uVar7 = (undefined2)((ulong)pbVar11 >> 0x10);
  pbVar4 = (byte *)pbVar11;
  if (iStack_4 == 0 && local_6 == 0) {
    *(undefined2 *)(iVar8 + 0x58) = pbVar4;
    *(undefined2 *)(iVar8 + 0x5a) = uVar7;
  }
  else {
    *(undefined2 *)(local_6 + 6) = pbVar4;
    *(undefined2 *)(local_6 + 8) = uVar7;
  }
  (pbVar4 + 8)[0] = 0;
  (pbVar4 + 8)[1] = 0;
  (pbVar4 + 6)[0] = 0;
  (pbVar4 + 6)[1] = 0;
  pbVar11[0] = 0;
  pbVar11[1] = 0;
  uVar10 = (undefined2)((ulong)param_2 >> 0x10);
  if (*param_2 == '^') {
    pcVar2 = (char *)param_2;
    param_2._0_2_ = (char *)param_2 + 1;
    param_2 = (char *)CONCAT22(uVar10,(char *)param_2);
    if (*(char *)param_2 == '^') {
      pbVar11[0] = 1;
      pbVar11[1] = 0;
      param_2 = (char *)CONCAT22(uVar10,pcVar2 + 2);
    }
    else {
      *pbVar11 = *pbVar11 | 2;
    }
  }
  iVar5 = iVar8 + 0x84;
  uVar10 = uVar9;
  FUN_2388_0dd4((char *)param_2,param_2._2_2_,iVar5,uVar9);
  uVar12 = FUN_1f45_010a(iVar5,uVar10);
  uVar10 = (undefined2)((ulong)uVar12 >> 0x10);
  *(int *)(pbVar4 + 2) = (int)uVar12;
  *(undefined2 *)(pbVar4 + 4) = uVar10;
  FUN_2388_0dec(*(undefined2 *)(pbVar4 + 2),uVar10,(char *)param_2,param_2._2_2_);
  *(int *)(iVar8 + 4) = *(int *)(iVar8 + 4) + 1;
  return pbVar4;
}



byte * __cdecl16far
FUN_133d_0d52(undefined4 param_1,undefined2 param_2,undefined2 param_3,int param_4,int param_5,
             int param_6)

{
  int iVar1;
  byte *pbVar2;
  int iVar3;
  undefined2 uVar4;
  int iVar5;
  int iVar6;
  undefined2 uVar7;
  int iVar8;
  undefined2 uVar9;
  undefined2 unaff_DS;
  byte *pbVar10;
  undefined4 uVar11;
  int local_c;
  int iStack_a;
  
  uVar9 = (undefined2)((ulong)param_1 >> 0x10);
  iVar8 = (int)param_1;
  local_c = 0;
  iStack_a = 0;
  iVar5 = *(int *)(iVar8 + 0x62);
  iVar3 = *(int *)(iVar8 + 0x60);
  while (iVar1 = iVar3, iVar6 = iVar5, iVar6 != 0 || iVar1 != 0) {
    iVar3 = *(int *)(iVar1 + 0x10);
    local_c = iVar1;
    iStack_a = iVar6;
    iVar5 = *(int *)(iVar1 + 0x12);
  }
  pbVar10 = (byte *)FUN_1f45_010a(iVar8 + 0x84,uVar9);
  uVar7 = (undefined2)((ulong)pbVar10 >> 0x10);
  pbVar2 = (byte *)pbVar10;
  if (iStack_a == 0 && local_c == 0) {
    *(undefined2 *)(iVar8 + 0x60) = pbVar2;
    *(undefined2 *)(iVar8 + 0x62) = uVar7;
  }
  else {
    *(undefined2 *)(local_c + 0x10) = pbVar2;
    *(undefined2 *)(local_c + 0x12) = uVar7;
  }
  (pbVar2 + 0x12)[0] = 0;
  (pbVar2 + 0x12)[1] = 0;
  (pbVar2 + 0x10)[0] = 0;
  (pbVar2 + 0x10)[1] = 0;
  pbVar10[0] = 0;
  pbVar10[1] = 0;
  iVar3 = iVar8 + 0x84;
  iVar5 = iVar3;
  uVar4 = uVar9;
  FUN_2388_0dd4(param_2,param_3,iVar3,uVar9);
  uVar11 = FUN_1f45_010a(iVar5,uVar4);
  uVar4 = (undefined2)((ulong)uVar11 >> 0x10);
  *(int *)(pbVar2 + 8) = (int)uVar11;
  *(undefined2 *)(pbVar2 + 10) = uVar4;
  FUN_2388_0dec(*(undefined2 *)(pbVar2 + 8),uVar4,param_2,param_3);
  FUN_2388_0e22(*(undefined2 *)(pbVar2 + 8),*(undefined2 *)(pbVar2 + 10),0x595);
  uVar4 = FUN_133d_0494(*(undefined2 *)(pbVar2 + 8),*(undefined2 *)(pbVar2 + 10),iVar8 + 0x74,uVar9)
  ;
  *(undefined2 *)(pbVar2 + 2) = uVar4;
  *(int *)(pbVar2 + 6) = param_6;
  uVar11 = FUN_1f45_010a(iVar3,uVar9);
  *(int *)(pbVar2 + 0xc) = (int)uVar11;
  *(int *)(pbVar2 + 0xe) = (int)((ulong)uVar11 >> 0x10);
  iVar5 = FUN_133d_0494(0x597,unaff_DS,iVar8 + 0x74,uVar9);
  *(int *)(pbVar2 + 4) = iVar5 * param_6;
  iVar5 = *(int *)(pbVar2 + 2) + *(int *)(pbVar2 + 4) + 10;
  if (iVar5 < *(int *)(iVar8 + 0x34)) {
    iVar5 = *(int *)(iVar8 + 0x34);
  }
  *(int *)(iVar8 + 0x34) = iVar5;
  if (param_5 == 0 && param_4 == 0) {
    **(undefined1 **)(pbVar2 + 0xc) = 0;
  }
  else {
    FUN_2388_0d58(*(undefined2 *)(pbVar2 + 0xc),*(undefined2 *)(pbVar2 + 0xe),param_4,param_5,
                  param_6);
    *(undefined1 *)((int)*(undefined4 *)(pbVar2 + 0xc) + param_6) = 0;
    iVar5 = FUN_2388_0dd4(*(undefined2 *)(pbVar2 + 0xc),*(undefined2 *)(pbVar2 + 0xe));
    if (iVar5 != 0) {
      *pbVar10 = *pbVar10 | 0x80;
    }
  }
  *(int *)(iVar8 + 8) = *(int *)(iVar8 + 8) + 1;
  return pbVar2;
}



char __cdecl16near FUN_133d_0f24(char *param_1)

{
  char cVar1;
  undefined2 unaff_DS;
  
  cVar1 = *param_1;
  if ((cVar1 == '\x06') && (*(int *)0x58a == 0)) {
    cVar1 = '\x05';
  }
  return cVar1;
}



int * __cdecl16far
FUN_133d_0f4a(undefined4 param_1,int param_2,int param_3,int param_4,int param_5,int param_6,
             int param_7)

{
  int iVar1;
  int *piVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined2 uVar6;
  int iVar7;
  undefined2 uVar8;
  int *piVar9;
  undefined4 uVar10;
  undefined2 uVar11;
  int local_14;
  int local_8;
  int iStack_6;
  int local_4;
  
  uVar8 = (undefined2)((ulong)param_1 >> 0x10);
  iVar7 = (int)param_1;
  local_8 = 0;
  iStack_6 = 0;
  iVar3 = *(int *)(iVar7 + 0x5e);
  iVar4 = *(int *)(iVar7 + 0x5c);
  while (iVar1 = iVar4, iVar5 = iVar3, iVar5 != 0 || iVar1 != 0) {
    iVar4 = *(int *)(iVar1 + 0x10);
    local_8 = iVar1;
    iStack_6 = iVar5;
    iVar3 = *(int *)(iVar1 + 0x12);
  }
  piVar9 = (int *)FUN_1f45_010a(iVar7 + 0x84,uVar8);
  uVar6 = (undefined2)((ulong)piVar9 >> 0x10);
  piVar2 = (int *)piVar9;
  if (iStack_6 == 0 && local_8 == 0) {
    *(undefined2 *)(iVar7 + 0x5c) = piVar2;
    *(undefined2 *)(iVar7 + 0x5e) = uVar6;
    *(undefined2 *)(iVar7 + 0x50) = piVar2;
    *(undefined2 *)(iVar7 + 0x52) = uVar6;
    *piVar9 = *(int *)(iVar7 + 0x46) + *(int *)(iVar7 + 0x4a);
  }
  else {
    iVar3 = *(int *)(local_8 + 2);
    *(undefined2 *)(local_8 + 0x10) = piVar2;
    *(undefined2 *)(local_8 + 0x12) = uVar6;
    *piVar9 = iVar3;
  }
  piVar2[9] = 0;
  piVar2[8] = 0;
  piVar2[6] = param_2;
  piVar2[7] = param_3;
  piVar2[2] = param_4;
  piVar2[3] = param_7;
  if (param_6 == 0 && param_5 == 0) {
    iVar3 = 0;
    piVar2[5] = 0;
    piVar2[4] = 0;
    local_14 = 0;
  }
  else {
    if (*(int *)(iVar7 + 0x56) == 0 && *(int *)(iVar7 + 0x54) == 0) {
      *(undefined2 *)(iVar7 + 0x28) = 0;
    }
    iVar3 = iVar7 + 0x84;
    uVar11 = uVar8;
    FUN_2388_0dd4(param_5,param_6,iVar3,uVar8);
    uVar10 = FUN_1f45_010a(iVar3,uVar11);
    iVar3 = (int)((ulong)uVar10 >> 0x10);
    piVar2[4] = (int)uVar10;
    piVar2[5] = iVar3;
    FUN_2388_0dec(piVar2[4],iVar3,param_5,param_6);
    iVar3 = FUN_133d_0494(param_5,param_6,iVar7 + 0x74,uVar8);
    local_14 = iVar3 + *(int *)(iVar7 + 0x46) + *(int *)(iVar7 + 0x32);
    iVar3 = FUN_133d_0f24(*(undefined2 *)(iVar7 + 0x80),*(undefined2 *)(iVar7 + 0x82));
  }
  if ((*(byte *)(iVar7 + 10) & 2) == 0) {
    param_2 = param_4 * 0xc + param_2;
    local_4 = *(int *)(param_2 + 0x3e);
    iVar4 = *(int *)(param_2 + 0x40);
  }
  else {
    iVar4 = 0x10;
    local_4 = 0x10;
  }
  if (iVar4 < iVar3) {
    iVar4 = iVar3;
  }
  piVar2[1] = iVar4 + *(int *)(iVar7 + 0x46) + *piVar9;
  iVar3 = *(int *)(iVar7 + 0x2e);
  if (*(int *)(iVar7 + 0x2e) < local_4) {
    iVar3 = local_4;
  }
  *(int *)(iVar7 + 0x2e) = iVar3;
  iVar3 = *(int *)(iVar7 + 0x30);
  if (*(int *)(iVar7 + 0x30) < local_14) {
    iVar3 = local_14;
  }
  *(int *)(iVar7 + 0x30) = iVar3;
  *(int *)(iVar7 + 6) = *(int *)(iVar7 + 6) + 1;
  return piVar2;
}



int __cdecl16far
FUN_133d_1138(undefined4 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  int iVar1;
  undefined2 uVar2;
  undefined4 uVar3;
  
  uVar3 = FUN_1f45_010a((int)param_1 + 0x84,param_1._2_2_);
  *(undefined2 *)((int)param_1 + 100) = (int)uVar3;
  *(undefined2 *)((int)param_1 + 0x66) = (int)((ulong)uVar3 >> 0x10);
  uVar2 = (undefined2)((ulong)*(undefined4 *)((int)param_1 + 100) >> 0x10);
  iVar1 = (int)*(undefined4 *)((int)param_1 + 100);
  *(undefined2 *)(iVar1 + 0xc) = param_2;
  *(undefined2 *)(iVar1 + 0xe) = param_3;
  *(undefined2 *)(iVar1 + 4) = param_4;
  return iVar1;
}



void FUN_133d_117a(undefined2 param_1,undefined2 param_2,int param_3,undefined2 param_4)

{
  FUN_133d_0546(param_1,param_2,param_3 + 0x74,param_4);
  return;
}



int FUN_133d_11a6(undefined4 param_1)

{
  int in_AX;
  undefined1 *puVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined2 uVar6;
  undefined2 uVar7;
  undefined2 unaff_SS;
  undefined4 uVar8;
  undefined4 uVar9;
  int local_168;
  undefined4 local_164;
  char local_160;
  undefined1 local_15f [79];
  int local_110;
  undefined4 local_10e;
  int local_10a;
  char local_108 [256];
  int local_8;
  undefined4 local_6;
  
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  iVar5 = (int)param_1;
  iVar4 = *(int *)(iVar5 + 0x5a);
  local_10e = (byte *)CONCAT22(iVar4,(byte *)*(undefined2 *)(iVar5 + 0x58));
  local_10a = -(*(int *)(iVar5 + 0x48) * 2 - *(int *)(iVar5 + 0x28));
  local_110 = *(int *)(iVar5 + 0x2c);
  local_108[0] = '\0';
  local_168 = 0;
  local_8 = 0;
  for (; iVar4 != 0 || (byte *)local_10e != (byte *)0x0;
      local_10e = (byte *)CONCAT22(iVar4,*(byte **)((byte *)local_10e + 6))) {
    uVar7 = (undefined2)((ulong)local_10e >> 0x10);
    iVar4 = *(int *)((byte *)local_10e + 4);
    local_6 = (char *)CONCAT22(iVar4,*(char **)((byte *)local_10e + 2));
    if ((*local_10e & 3) != 0) {
      if (local_108[0] != '\0') {
        if (in_AX != 0) {
          FUN_133d_117a(local_108,unaff_SS,iVar5,uVar6);
        }
        iVar4 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
        local_168 = local_168 + iVar4 + 1;
        iVar4 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
        local_110 = local_110 + iVar4 + 1;
        local_108[0] = '\0';
        local_8 = 0;
      }
      if (in_AX != 0) {
        if ((*local_10e & 1) != 0) {
          FUN_133d_0494((char *)local_6,local_6._2_2_,iVar5 + 0x74,uVar6);
        }
        FUN_133d_117a((char *)local_6,local_6._2_2_,iVar5,uVar6);
      }
      while (*local_6 != '\0') {
        local_6 = (char *)CONCAT22(local_6._2_2_,(char *)local_6 + 1);
      }
      iVar4 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
      local_168 = local_168 + iVar4 + 1;
      uVar8 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
      iVar4 = (int)((ulong)uVar8 >> 0x10);
      local_110 = local_110 + (int)uVar8 + 1;
    }
    while( true ) {
      iVar3 = FUN_2388_0dd4((char *)local_6,local_6._2_2_);
      if (iVar3 == 0) break;
      while( true ) {
        uVar7 = (undefined2)((ulong)local_6 >> 0x10);
        if (*local_6 != ' ') break;
        local_6 = (char *)CONCAT22(uVar7,(char *)local_6 + 1);
      }
      puVar1 = (undefined1 *)FUN_2388_0ca8((char *)local_6,uVar7,0x20);
      local_164 = (undefined1 *)CONCAT22(iVar4,puVar1);
      if (iVar4 != 0 || puVar1 != (undefined1 *)0x0) {
        *local_164 = 0;
      }
      iVar3 = FUN_2388_0dd4((char *)local_6,local_6._2_2_);
      local_160 = '\0';
      if (local_108[0] != '\0') {
        FUN_2388_05e6(&local_160,0x599);
      }
      FUN_2388_0e22(&local_160);
      uVar8 = FUN_133d_0494(&local_160,unaff_SS,(int *)(iVar5 + 0x74),uVar6);
      iVar2 = (int)((ulong)uVar8 >> 0x10);
      if (local_10a < *(int *)(iVar5 + 0x74) + (int)uVar8 + local_8) {
        if (in_AX != 0) {
          FUN_133d_117a(local_108,unaff_SS,iVar5,uVar6);
        }
        iVar2 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
        local_168 = local_168 + iVar2 + 1;
        uVar9 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
        iVar2 = (int)((ulong)uVar9 >> 0x10);
        local_110 = local_110 + (int)uVar9 + 1;
        while (local_160 == ' ') {
          FUN_2388_0626(&local_160,local_15f);
        }
        local_108[0] = '\0';
        local_8 = 0;
      }
      FUN_2388_0e22(local_108);
      local_8 = local_8 + *(int *)(iVar5 + 0x74) + (int)uVar8;
      if (iVar4 != 0 || puVar1 != (undefined1 *)0x0) {
        *local_164 = 0x20;
      }
      local_6 = (char *)CONCAT22(local_6._2_2_,(char *)local_6 + iVar3);
      iVar4 = iVar2;
    }
    uVar7 = (undefined2)((ulong)local_10e >> 0x10);
    iVar4 = *(int *)((byte *)local_10e + 8);
  }
  if (local_108[0] != '\0') {
    if (in_AX != 0) {
      FUN_133d_117a(local_108,unaff_SS,iVar5,uVar6);
    }
    iVar4 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
    local_168 = local_168 + iVar4 + 1;
    FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
  }
  return local_168;
}



undefined2 FUN_133d_14d4(undefined4 param_1)

{
  int *piVar1;
  int *piVar2;
  undefined2 uVar3;
  bool bVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  undefined2 uVar14;
  undefined2 uVar15;
  undefined2 unaff_DS;
  int local_2e;
  int local_2a;
  int local_20;
  int local_1a;
  int local_16;
  undefined4 local_14;
  int local_10;
  undefined2 local_a;
  int local_8;
  int local_6;
  
  local_a = 1;
  local_2e = 0;
  local_16 = 0;
  local_20 = 0;
  uVar14 = (undefined2)((ulong)param_1 >> 0x10);
  iVar11 = (int)param_1;
  if ((*(int *)(iVar11 + 8) != 0) && (*(int *)(iVar11 + 2) != 0)) {
    *(undefined2 *)(iVar11 + 8) = 0;
    *(undefined2 *)(iVar11 + 0x62) = 0;
    *(undefined2 *)(iVar11 + 0x60) = 0;
  }
  *(undefined2 *)(iVar11 + 0x10) = *(undefined2 *)(iVar11 + 0xc);
  *(undefined2 *)(iVar11 + 0x12) = *(undefined2 *)(iVar11 + 0xe);
  *(undefined2 *)(iVar11 + 0x14) = 0;
  *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x4a) * 2 + *(int *)(iVar11 + 0x46);
  uVar5 = -(uint)((*(byte *)(iVar11 + 10) & 0x10) == 0) & 3;
  *(uint *)(iVar11 + 0x2a) = uVar5;
  iVar6 = uVar5 + *(int *)(iVar11 + 0x46);
  *(int *)(iVar11 + 0x2c) = iVar6;
  *(uint *)(iVar11 + 0x24) = uVar5;
  *(int *)(iVar11 + 0x26) = iVar6;
  iVar6 = *(int *)(iVar11 + 0x28);
  if (iVar6 < *(int *)(iVar11 + 0x20)) {
    iVar6 = *(int *)(iVar11 + 0x20);
  }
  if (iVar6 < *(int *)(iVar11 + 0x34)) {
    iVar6 = *(int *)(iVar11 + 0x34);
  }
  *(int *)(iVar11 + 0x28) = iVar6;
  *(int *)(iVar11 + 0x34) = iVar6;
  *(int *)(iVar11 + 0x20) = iVar6;
  if (*(int *)(iVar11 + 2) + *(int *)(iVar11 + 4) + *(int *)(iVar11 + 6) + *(int *)(iVar11 + 8) != 0
     ) {
    local_8 = 0;
    if (*(int *)(iVar11 + 0x5e) != 0 || *(int *)(iVar11 + 0x5c) != 0) {
      iVar6 = *(int *)(iVar11 + 0x48) + *(int *)(iVar11 + 0x2e) + *(int *)(iVar11 + 0x30) +
              *(int *)(iVar11 + 0x32);
      *(int *)(iVar11 + 0x14) = *(int *)(iVar11 + 0x14) + iVar6;
      *(int *)(iVar11 + 0x2a) = *(int *)(iVar11 + 0x2a) + iVar6;
      *(int *)(iVar11 + 0x24) = *(int *)(iVar11 + 0x24) + iVar6;
      *(int *)(iVar11 + 0x36) = *(int *)(iVar11 + 0x36) + iVar6;
      iVar6 = *(int *)(iVar11 + 0x5e);
      iVar7 = *(int *)(iVar11 + 0x5c);
      while (iVar6 != 0 || iVar7 != 0) {
        local_8 = *(int *)(iVar7 + 2);
        piVar1 = (int *)(iVar7 + 0x10);
        iVar6 = *(int *)(iVar7 + 0x12);
        iVar7 = *piVar1;
      }
    }
    if (*(int *)(iVar11 + 0x5a) != 0 || *(int *)(iVar11 + 0x58) != 0) {
      local_2e = FUN_133d_11a6(iVar11,uVar14);
      iVar6 = *(int *)(iVar11 + 0x46);
      *(int *)(iVar11 + 0x26) = *(int *)(iVar11 + 0x26) + iVar6 + local_2e;
      *(int *)(iVar11 + 0x38) = *(int *)(iVar11 + 0x38) + iVar6 + local_2e;
    }
    if (*(int *)(iVar11 + 0x56) != 0 || *(int *)(iVar11 + 0x54) != 0) {
      iVar6 = FUN_133d_0f24(*(undefined2 *)(iVar11 + 0x80),*(undefined2 *)(iVar11 + 0x82));
      local_16 = (iVar6 + *(int *)(iVar11 + 0x46)) * *(int *)(iVar11 + 2);
    }
    if (*(int *)(iVar11 + 0x62) != 0 || *(int *)(iVar11 + 0x60) != 0) {
      iVar6 = FUN_133d_0f24(*(undefined2 *)(iVar11 + 0x80),*(undefined2 *)(iVar11 + 0x82));
      local_20 = (iVar6 + *(int *)(iVar11 + 0x46) + 5) * *(int *)(iVar11 + 8);
    }
    local_10 = local_20 + local_16 + local_2e;
    if (local_10 != 0) {
      local_10 = local_10 + *(int *)(iVar11 + 0x46);
    }
    iVar6 = (-(uint)((*(byte *)(iVar11 + 10) & 0x10) == 0) & 3) * 2;
    if (local_8 < local_10) {
      local_8 = local_10;
    }
    *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + iVar6 + local_8;
    *(int *)(iVar11 + 0x14) = *(int *)(iVar11 + 0x14) + iVar6 + *(int *)(iVar11 + 0x20);
    if (*(int *)0x566 != 0) {
      if ((*(int *)(iVar11 + 0x80) == *(int *)0x80) && (*(int *)(iVar11 + 0x82) == *(int *)0x82)) {
        *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 6;
      }
      else {
        *(int *)(iVar11 + 0x16) = *(int *)(iVar11 + 0x16) + 3;
      }
    }
    if (*(int *)(iVar11 + 0x10) == -1) {
      *(int *)(iVar11 + 0x10) = -((*(int *)(iVar11 + 0x14) >> 1) + -0xa0);
    }
    if (*(int *)(iVar11 + 0x12) == -1) {
      *(int *)(iVar11 + 0x12) = -((*(int *)(iVar11 + 0x16) >> 1) + -100);
    }
    iVar6 = *(int *)(iVar11 + 0x16) + *(int *)(iVar11 + 0x12);
    iVar7 = *(int *)(iVar11 + 0x14) + *(int *)(iVar11 + 0x10);
    if (0x140 < iVar7) {
      *(int *)(iVar11 + 0x10) = *(int *)(iVar11 + 0x10) - (iVar7 + -0x140);
    }
    if (200 < iVar6) {
      *(int *)(iVar11 + 0x12) = *(int *)(iVar11 + 0x12) + (200 - iVar6);
    }
    if ((*(int *)(iVar11 + 0x10) < 0) || (*(int *)(iVar11 + 0x12) < 0)) {
      FUN_1ed0_03d6(*(int *)(iVar11 + 0x12),*(int *)(iVar11 + 0x12) >> 0xf,*(int *)(iVar11 + 0x10),
                    *(int *)(iVar11 + 0x10) >> 0xf);
    }
    *(undefined2 *)(iVar11 + 0x18) = *(undefined2 *)(iVar11 + 0x10);
    *(undefined2 *)(iVar11 + 0x1a) = *(undefined2 *)(iVar11 + 0x12);
    iVar8 = *(int *)(iVar11 + 0x14);
    *(int *)(iVar11 + 0x1c) = iVar8;
    *(undefined2 *)(iVar11 + 0x1e) = *(undefined2 *)(iVar11 + 0x16);
    if (*(int *)(iVar11 + 0x6a) != 0 || *(int *)(iVar11 + 0x68) != 0) {
      piVar2 = (int *)*(int *)(iVar11 + 0x68);
      uVar3 = *(undefined2 *)(iVar11 + 0x6a);
      local_14 = (int *)CONCAT22(uVar3,piVar2);
      if (-1 < *(int *)0x55c) {
        uVar15 = (undefined2)((ulong)*(undefined4 *)(piVar2 + 6) >> 0x10);
        iVar12 = (int)*(undefined4 *)(piVar2 + 6);
        iVar10 = *(int *)(iVar12 + 0x4c) + 3;
        local_1a = -3;
        iVar12 = *(int *)(iVar12 + 0x4a) + 3;
        iVar8 = iVar8 + iVar12;
        local_2a = iVar8 + 3;
        if (0x140 < local_2a) {
          local_1a = iVar8 + -0x140;
          local_2a = 0x140;
        }
        if ((((*(int *)0x55c == 0) || (*(int *)0x55c == 3)) || (*(int *)0x55c == 5)) ||
           ((*(int *)0x55c == 7 || (*(int *)0x55c == 8)))) {
          bVar4 = true;
        }
        else {
          bVar4 = false;
        }
        *(int *)(iVar11 + 0x1c) = local_2a;
        if (bVar4) {
          iVar8 = -((local_2a >> 1) + -0xa0);
          *(int *)(iVar11 + 0x18) = iVar8;
          piVar2[2] = iVar8;
          *(int *)(iVar11 + 0x10) = (iVar8 - local_1a) + iVar12;
        }
        else {
          iVar8 = -((local_2a >> 1) + -0xa0);
          *(int *)(iVar11 + 0x18) = iVar8;
          *(int *)(iVar11 + 0x10) = iVar8;
          piVar2[2] = (iVar8 + *(int *)(iVar11 + 0x14)) - local_1a;
        }
        iVar12 = (iVar10 >> 1) + -100;
        iVar13 = -iVar12;
        *local_14 = iVar13;
        iVar8 = iVar13;
        if (-*(int *)(iVar11 + 0x12) != iVar12 && *(int *)(iVar11 + 0x12) <= iVar13) {
          iVar8 = *(int *)(iVar11 + 0x12);
        }
        *(int *)(iVar11 + 0x1a) = iVar8;
        iVar10 = iVar13 + iVar10 + -1;
        if (iVar10 < iVar6) {
          iVar10 = iVar6;
        }
        *(int *)(iVar11 + 0x1e) = (iVar10 - iVar8) + 1;
      }
      if ((-1 < *(int *)0x55e) || (-1 < *(int *)0x560)) {
        uVar15 = (undefined2)((ulong)*(undefined4 *)(piVar2 + 6) >> 0x10);
        iVar13 = (int)*(undefined4 *)(piVar2 + 6);
        iVar6 = *(int *)(iVar13 + 0x4a);
        iVar8 = *(int *)(iVar13 + 0x4c);
        iVar10 = *(int *)(iVar13 + 0x10);
        iVar12 = *(int *)(iVar13 + 0x12);
        local_6 = *(int *)(iVar13 + 0x14);
        iVar13 = iVar10;
        if (iVar8 < iVar10) {
          iVar13 = iVar8;
        }
        iVar13 = (iVar8 - iVar13) + *(int *)(iVar11 + 0x16);
        if (iVar13 < 200) {
          iVar9 = -((iVar13 >> 1) + -100);
          *local_14 = iVar9;
          *(int *)(iVar11 + 0x1a) = iVar9;
          if (iVar13 < iVar8) {
            iVar13 = iVar8;
          }
          *(int *)(iVar11 + 0x1e) = iVar13;
          *(int *)(iVar11 + 0x12) = (iVar9 - iVar10) + iVar8;
          if (iVar12 == 0) {
            local_2a = (*(int *)(iVar11 + 0x14) - local_6) + iVar6;
            if (0x140 < local_2a) {
              local_6 = local_6 + local_2a + -0x140;
              local_2a = 0x140;
            }
            iVar7 = -((local_2a >> 1) + -0xa0);
            piVar2[2] = iVar7;
            *(int *)(iVar11 + 0x18) = iVar7;
            *(int *)(iVar11 + 0x1c) = local_2a;
            *(int *)(iVar11 + 0x10) = (iVar7 - local_6) + iVar6;
          }
          else if (iVar12 == 1) {
            iVar10 = -((iVar6 >> 1) + -0xa0);
            piVar2[2] = iVar10;
            iVar8 = *(int *)(iVar11 + 0x10);
            if (iVar10 < *(int *)(iVar11 + 0x10)) {
              iVar8 = iVar10;
            }
            *(int *)(iVar11 + 0x18) = iVar8;
            iVar6 = iVar10 + iVar6 + -1;
            if (iVar6 < iVar7) {
              iVar6 = iVar7;
            }
            *(int *)(iVar11 + 0x1c) = (iVar6 - iVar8) + 1;
          }
          else if (iVar12 == 2) {
            local_2a = (*(int *)(iVar11 + 0x14) - local_6) + iVar6;
            if (0x140 < local_2a) {
              local_6 = local_6 + local_2a + -0x140;
              local_2a = 0x140;
            }
            iVar6 = -((local_2a >> 1) + -0xa0);
            *(int *)(iVar11 + 0x10) = iVar6;
            *(int *)(iVar11 + 0x18) = iVar6;
            *(int *)(iVar11 + 0x1c) = local_2a;
            piVar2[2] = (iVar6 + *(int *)(iVar11 + 0x14)) - local_6;
          }
        }
        else {
          *(byte *)(iVar11 + 10) = *(byte *)(iVar11 + 10) | 0x40;
        }
      }
    }
    iVar6 = *(int *)(iVar11 + 0x10);
    *(int *)(iVar11 + 0x24) = *(int *)(iVar11 + 0x24) + iVar6;
    iVar7 = *(int *)(iVar11 + 0x12);
    *(int *)(iVar11 + 0x26) = *(int *)(iVar11 + 0x26) + iVar7;
    *(int *)(iVar11 + 0x2a) = *(int *)(iVar11 + 0x2a) + iVar6;
    *(int *)(iVar11 + 0x2c) = *(int *)(iVar11 + 0x2c) + iVar7;
    *(int *)(iVar11 + 0x36) = *(int *)(iVar11 + 0x36) + iVar6;
    *(int *)(iVar11 + 0x38) = *(int *)(iVar11 + 0x38) + iVar7;
    local_a = 0;
  }
  return local_a;
}



void FUN_133d_1a4a(undefined4 param_1)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  
  iVar1 = (int)param_1;
  uVar2 = (undefined2)((ulong)param_1 >> 0x10);
  if ((*(int *)0x56a == 0) || ((*(byte *)(iVar1 + 10) & 0x20) == 0)) {
    FUN_1c34_0044(*(undefined2 *)(iVar1 + 0x1e),*(undefined2 *)(iVar1 + 0x1c),
                  *(undefined2 *)(iVar1 + 0x1a));
  }
  return;
}



void __cdecl16far FUN_133d_1a86(undefined4 param_1)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 uVar4;
  undefined2 unaff_DS;
  undefined1 local_54 [80];
  int local_4;
  
  if (*(int *)0x566 != 0) {
    uVar4 = (undefined2)((ulong)param_1 >> 0x10);
    iVar3 = (int)param_1;
    local_4 = *(int *)(iVar3 + 0x14) + *(int *)(iVar3 + 0x10) + -2;
    iVar1 = *(int *)(iVar3 + 0x16) + *(int *)(iVar3 + 0x12);
    iVar2 = iVar1 + -7;
    if ((*(int *)(iVar3 + 0x80) == *(int *)0x80) && (*(int *)(iVar3 + 0x82) == *(int *)0x82)) {
      iVar2 = iVar1 + -9;
    }
    local_54[0] = 0;
    FUN_18ad_00e2(local_54,*(undefined2 *)0x53b4,iVar2);
    FUN_18ad_02c2(local_54);
  }
  return;
}



void __cdecl16far FUN_133d_1af6(undefined4 param_1)

{
  undefined2 *puVar1;
  int iVar2;
  int iVar3;
  undefined2 uVar4;
  undefined2 uVar5;
  undefined2 unaff_DS;
  
  iVar3 = *(int *)0x55c;
  uVar4 = (undefined2)((ulong)param_1 >> 0x10);
  iVar2 = (int)param_1;
  if ((*(int *)(iVar2 + 0x6a) != 0 || *(int *)(iVar2 + 0x68) != 0) &&
     ((*(byte *)(iVar2 + 10) & 0x40) == 0)) {
    puVar1 = (undefined2 *)*(int *)(iVar2 + 0x68);
    uVar4 = *(undefined2 *)(iVar2 + 0x6a);
    FUN_1d8f_0000(*puVar1,puVar1[6],puVar1[7]);
    if ((7 < iVar3) && ((*(int *)0x56e != 0 && (0 < *(int *)0x4e7e)))) {
      uVar5 = (undefined2)((ulong)*(undefined4 *)(puVar1 + 8) >> 0x10);
      iVar3 = (int)*(undefined4 *)(puVar1 + 8);
      FUN_1d8f_0000(*puVar1,*(undefined2 *)(iVar3 + 0xc),*(undefined2 *)(iVar3 + 0xe));
    }
  }
  return;
}



void FUN_133d_1b8a(undefined4 param_1)

{
  int in_AX;
  int iVar1;
  undefined1 uVar4;
  int iVar2;
  int iVar3;
  int extraout_DX;
  int iVar5;
  undefined2 uVar6;
  undefined2 unaff_DS;
  undefined2 uVar7;
  undefined2 uVar8;
  undefined2 uVar9;
  undefined2 uVar10;
  undefined4 local_12;
  int local_c;
  
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  iVar5 = (int)param_1;
  local_c = *(int *)(iVar5 + 0x26);
  local_12 = (byte *)CONCAT22(*(undefined2 *)(iVar5 + 0x56),(byte *)*(undefined2 *)(iVar5 + 0x54));
  if (in_AX != 0) {
    uVar10 = *(undefined2 *)0x3afa;
    uVar9 = *(undefined2 *)0x3af8;
    uVar8 = *(undefined2 *)0x3af6;
    uVar7 = *(undefined2 *)0x3af4;
    iVar1 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82),uVar7,uVar8,
                          uVar9,uVar10);
    iVar1 = (iVar1 + *(int *)(iVar5 + 0x46)) * *(int *)(iVar5 + 2);
    uVar4 = (undefined1)((uint)iVar1 >> 8);
    FUN_133d_034a(0,0,CONCAT11(uVar4,*(undefined1 *)(iVar5 + 0x3e)),
                  CONCAT11(uVar4,*(undefined1 *)(iVar5 + 0x3c)),*(undefined2 *)(iVar5 + 0x14),
                  *(undefined2 *)(iVar5 + 0x12),*(undefined2 *)(iVar5 + 0x10),iVar1,uVar7,uVar8,
                  uVar9,uVar10);
  }
  while (local_12._2_2_ != 0 || (byte *)local_12 != (byte *)0x0) {
    *(uint *)0x562 = *local_12 & 2;
    if (((byte *)*(undefined2 *)(iVar5 + 0x4c) == (byte *)local_12) &&
       (*(int *)(iVar5 + 0x4e) == local_12._2_2_)) {
      uVar10 = *(undefined2 *)0x3afa;
      uVar9 = *(undefined2 *)0x3af8;
      uVar8 = *(undefined2 *)0x3af6;
      uVar7 = *(undefined2 *)0x3af4;
      iVar1 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82),uVar7,uVar8,
                            uVar9,uVar10);
      uVar4 = (undefined1)((uint)(iVar1 + 2) >> 8);
      FUN_133d_034a(0,0,CONCAT11(uVar4,*(undefined1 *)(iVar5 + 0x42)),
                    CONCAT11(uVar4,*(undefined1 *)(iVar5 + 0x40)),*(undefined2 *)(iVar5 + 0x14),
                    *(undefined2 *)(iVar5 + 0x12),*(undefined2 *)(iVar5 + 0x10),iVar1 + 2,uVar7,
                    uVar8,uVar9,uVar10);
    }
    if (**(char **)((byte *)local_12 + 8) == '\0') {
      iVar1 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
      FUN_1c5b_0004(CONCAT11((char)((uint)((iVar1 >> 1) + local_c) >> 8),
                             *(undefined1 *)(iVar5 + 0x76)),1,*(undefined2 *)0x3af4,
                    *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
    }
    else {
      if ((*(byte *)(iVar5 + 10) & 4) != 0) {
        *(char *)((int)*(undefined4 *)((byte *)local_12 + 8) + 1) =
             (-(*(int *)((byte *)local_12 + 6) == 0) & 0xfeU) + 0x5d;
      }
      iVar2 = iVar5 + 0x74;
      FUN_133d_0546(*(undefined2 *)((byte *)local_12 + 8),*(undefined2 *)((byte *)local_12 + 10),
                    iVar2,uVar6);
      iVar1 = extraout_DX;
      iVar3 = FUN_2388_0ca8(*(undefined2 *)((byte *)local_12 + 8),
                            *(undefined2 *)((byte *)local_12 + 10),0x7c);
      if (iVar1 != 0 || iVar3 != 0) {
        FUN_133d_0494(iVar3 + 1,iVar1,iVar2,uVar6);
        FUN_133d_0546(iVar3 + 1,iVar1,iVar2,uVar6);
      }
    }
    iVar1 = FUN_133d_0f24(*(undefined2 *)(iVar5 + 0x80),*(undefined2 *)(iVar5 + 0x82));
    local_c = local_c + iVar1 + *(int *)(iVar5 + 0x46);
    local_12 = (byte *)CONCAT22(*(undefined2 *)((byte *)local_12 + 0x12),
                                *(byte **)((byte *)local_12 + 0x10));
  }
  if (in_AX != 0) {
    if (*(int *)0x55c < 0) {
      FUN_133d_1af6(iVar5,uVar6);
    }
    FUN_133d_1a86(iVar5,uVar6);
    FUN_133d_1a4a(iVar5,uVar6);
  }
  return;
}



void FUN_133d_1e22(undefined4 param_1)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int in_AX;
  byte *pbVar4;
  int iVar5;
  int iVar6;
  undefined1 uVar7;
  int iVar8;
  undefined2 uVar9;
  undefined2 uVar10;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 local_5e [80];
  undefined4 local_e;
  int local_a;
  int local_8;
  int local_6;
  int local_4;
  
  *(undefined2 *)0x562 = 0;
  uVar9 = (undefined2)((ulong)param_1 >> 0x10);
  iVar8 = (int)param_1;
  iVar1 = *(int *)(iVar8 + 0x24);
  iVar2 = *(int *)(iVar8 + 0x48);
  iVar3 = *(int *)(iVar8 + 0x26);
  pbVar4 = (byte *)*(undefined2 *)(iVar8 + 0x60);
  iVar5 = *(int *)(iVar8 + 0x62);
  while( true ) {
    local_e = (byte *)CONCAT22(iVar5,pbVar4);
    if (iVar5 == 0 && pbVar4 == (byte *)0x0) break;
    FUN_2388_0dec(local_5e);
    iVar5 = FUN_2388_0684(local_5e);
    if (iVar5 < *(int *)((byte *)local_e + 6)) {
      FUN_2388_05e6(local_5e,0x59b);
    }
    uVar10 = (undefined2)((ulong)local_e >> 0x10);
    FUN_133d_0546(*(undefined2 *)((byte *)local_e + 8),*(undefined2 *)((byte *)local_e + 10),
                  iVar8 + 0x74,uVar9);
    local_6 = *(int *)((byte *)local_e + 2) + iVar1 + iVar2;
    local_a = iVar3;
    iVar5 = FUN_133d_0f24(*(undefined2 *)(iVar8 + 0x80),*(undefined2 *)(iVar8 + 0x82));
    iVar6 = iVar5 + iVar3 + 4;
    FUN_1c86_000c(CONCAT11((char)((uint)iVar6 >> 8),*(undefined1 *)(iVar8 + 0x76)),iVar6,
                  *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                  *(undefined2 *)0x3afa);
    local_4 = local_6 + 2;
    local_8 = iVar3 + 2;
    uVar7 = (undefined1)((uint)local_4 >> 8);
    FUN_133d_034a(0,0,CONCAT11(uVar7,*(undefined1 *)(iVar8 + 0x3e)),
                  CONCAT11(uVar7,*(undefined1 *)(iVar8 + 0x3c)),*(undefined2 *)(iVar8 + 0x14),
                  *(undefined2 *)(iVar8 + 0x12),*(undefined2 *)(iVar8 + 0x10),iVar5 + 1,
                  *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                  *(undefined2 *)0x3afa);
    uVar10 = (undefined2)((ulong)local_e >> 0x10);
    if ((*local_e & 0x80) != 0) {
      iVar6 = FUN_133d_0494(*(undefined2 *)((byte *)local_e + 0xc),
                            *(undefined2 *)((byte *)local_e + 0xe),iVar8 + 0x74,uVar9);
      uVar7 = (undefined1)((uint)(iVar6 + 2) >> 8);
      FUN_133d_034a(0,0,CONCAT11(uVar7,*(undefined1 *)(iVar8 + 0x42)),
                    CONCAT11(uVar7,*(undefined1 *)(iVar8 + 0x40)),*(undefined2 *)(iVar8 + 0x14),
                    *(undefined2 *)(iVar8 + 0x12),*(undefined2 *)(iVar8 + 0x10),iVar5 + 1,
                    *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                    *(undefined2 *)0x3afa);
    }
    FUN_133d_0546(local_5e,unaff_SS,iVar8 + 0x74,uVar9);
    uVar10 = (undefined2)((ulong)local_e >> 0x10);
    pbVar4 = *(byte **)((byte *)local_e + 0x10);
    iVar5 = *(int *)((byte *)local_e + 0x12);
  }
  if (in_AX != 0) {
    FUN_133d_1a86(iVar8,uVar9);
    FUN_133d_1a4a(iVar8,uVar9);
  }
  return;
}



void FUN_133d_202c(undefined4 param_1)

{
  int *piVar1;
  int in_AX;
  int iVar2;
  undefined1 uVar3;
  int in_DX;
  undefined2 uVar4;
  int iVar5;
  int in_BX;
  int iVar6;
  undefined2 uVar7;
  undefined2 unaff_DS;
  undefined4 local_e;
  
  *(undefined2 *)0x562 = 0;
  uVar7 = (undefined2)((ulong)param_1 >> 0x10);
  iVar6 = (int)param_1;
  if (*(int *)(iVar6 + 0x5e) != 0 || *(int *)(iVar6 + 0x5c) != 0) {
    uVar4 = *(undefined2 *)(iVar6 + 0x5e);
    iVar2 = *(int *)(iVar6 + 0x5c);
    while (*(int *)(iVar2 + 0x12) != 0 || *(int *)(iVar2 + 0x10) != 0) {
      piVar1 = (int *)(iVar2 + 0x10);
      uVar4 = *(undefined2 *)(iVar2 + 0x12);
      iVar2 = *piVar1;
    }
    local_e = (int *)CONCAT22(*(undefined2 *)(iVar6 + 0x5e),(int *)*(undefined2 *)(iVar6 + 0x5c));
    iVar5 = (-(uint)((*(uint *)(iVar6 + 10) & 0x10) == 0) & 3) + *(int *)(iVar6 + 0x12);
    if (in_DX != 0) {
      uVar3 = (undefined1)(*(uint *)(iVar6 + 10) >> 8);
      FUN_133d_034a(0,0,CONCAT11(uVar3,*(undefined1 *)(iVar6 + 0x3e)),
                    CONCAT11(uVar3,*(undefined1 *)(iVar6 + 0x3c)),*(undefined2 *)(iVar6 + 0x14),
                    *(undefined2 *)(iVar6 + 0x12),*(undefined2 *)(iVar6 + 0x10),
                    (*(int *)(iVar2 + 2) - *local_e) + 3,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6
                    ,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
    }
    while( true ) {
      if (local_e._2_2_ == 0 && (int *)local_e == (int *)0x0) break;
      if ((*(byte *)(iVar6 + 10) & 2) == 0) {
        FUN_1d8f_0000(*local_e + iVar5,((int *)local_e)[6],((int *)local_e)[7]);
      }
      else {
        FUN_1000_0008(100,0x10,*local_e + iVar5);
      }
      if (((((int *)*(undefined2 *)(iVar6 + 0x50) == (int *)local_e) &&
           (*(int *)(iVar6 + 0x52) == local_e._2_2_)) && (in_BX != 0)) &&
         (((*(byte *)(iVar6 + 10) & 0x80) != 0 &&
          (*(int *)(iVar6 + 0x66) != 0 || *(int *)(iVar6 + 100) != 0)))) {
        FUN_1c86_000c(0xf,*local_e + iVar5 + 0x10,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                      *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
      }
      if ((in_AX != 0) && (((int *)local_e)[5] != 0 || ((int *)local_e)[4] != 0)) {
        FUN_133d_0f24(*(undefined2 *)(iVar6 + 0x80),*(undefined2 *)(iVar6 + 0x82));
        FUN_133d_0546(((int *)local_e)[4],((int *)local_e)[5],iVar6 + 0x74,uVar7);
      }
      local_e = (int *)CONCAT22(((int *)local_e)[9],(int *)((int *)local_e)[8]);
    }
    if (in_DX != 0) {
      FUN_133d_1a86(iVar6,uVar7);
      FUN_133d_1a4a(iVar6,uVar7);
    }
  }
  return;
}



void __cdecl16far
FUN_133d_2286(undefined4 param_1,undefined2 param_2,int param_3,undefined2 param_4,int param_5)

{
  int iVar1;
  undefined1 extraout_AH;
  undefined1 extraout_AH_00;
  undefined1 extraout_AH_01;
  undefined1 extraout_AH_02;
  uint uVar2;
  int iVar3;
  undefined1 uVar4;
  undefined2 unaff_DS;
  
  iVar1 = param_3;
  if (param_1._2_2_ != 0 || (int)param_1 != 0) {
    param_2 = *(undefined2 *)((int)param_1 + 0x10);
    iVar1 = *(int *)((int)param_1 + 0x12);
    param_4 = *(undefined2 *)((int)param_1 + 0x14);
    if ((*(byte *)((int)param_1 + 10) & 0x10) != 0) goto LAB_133d_23d2;
  }
  FUN_1c86_000c(0,param_5 + param_3 + -1,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  iVar3 = param_5 + param_3 + -2;
  FUN_1c86_000c(CONCAT11((char)((uint)iVar3 >> 8),*(undefined1 *)0x544),iVar3,*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  FUN_1c80_0000(CONCAT11(extraout_AH,*(undefined1 *)0x548),*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  FUN_1c80_0000(CONCAT11(extraout_AH_00,*(undefined1 *)0x546),*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  FUN_1c79_0006(CONCAT11(extraout_AH_01,*(undefined1 *)0x546),*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  FUN_1c79_0006(CONCAT11(extraout_AH_02,*(undefined1 *)0x548),*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
LAB_133d_23d2:
  if (param_1._2_2_ != 0 || (int)param_1 != 0) {
    uVar2 = -(uint)((*(byte *)((int)param_1 + 10) & 0x10) == 0) & 3;
  }
  else {
    uVar2 = 3;
  }
  iVar3 = -(uVar2 * 2 - param_5);
  uVar4 = (undefined1)((uint)iVar3 >> 8);
  FUN_133d_034a(0,0,CONCAT11(uVar4,*(undefined1 *)0x53e),CONCAT11(uVar4,*(undefined1 *)0x53c),
                param_4,iVar1,param_2,iVar3,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  return;
}



undefined2 __cdecl16far FUN_133d_249c(undefined4 param_1)

{
  undefined2 uVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  undefined2 uVar4;
  int iVar5;
  undefined2 unaff_DS;
  undefined2 local_58;
  
  local_58 = 1;
  iVar5 = FUN_133d_14d4((int)param_1,param_1._2_2_);
  if (iVar5 == 0) {
    *(undefined2 *)0x562 = 0;
    uVar1 = *(undefined2 *)((int)param_1 + 0x10);
    uVar2 = *(undefined2 *)((int)param_1 + 0x12);
    uVar3 = *(undefined2 *)((int)param_1 + 0x14);
    uVar4 = *(undefined2 *)((int)param_1 + 0x16);
    if (-1 < *(int *)0x55c) {
      FUN_133d_1af6((int)param_1,param_1._2_2_);
    }
    FUN_133d_2286((int)param_1,param_1._2_2_,uVar1,uVar2,uVar3,uVar4);
    FUN_133d_202c((int)param_1,param_1._2_2_);
    *(undefined2 *)0x562 = 0;
    FUN_133d_11a6((int)param_1,param_1._2_2_);
    FUN_133d_1b8a((int)param_1,param_1._2_2_);
    FUN_133d_1e22((int)param_1,param_1._2_2_);
    if (*(int *)0x55c < 0) {
      FUN_133d_1af6((int)param_1,param_1._2_2_);
    }
    FUN_133d_1a86((int)param_1,param_1._2_2_);
    FUN_133d_1a4a((int)param_1,param_1._2_2_);
    local_58 = 0;
  }
  return local_58;
}



void __cdecl16far FUN_133d_256c(undefined4 param_1,undefined2 param_2,undefined2 param_3)

{
  int iVar1;
  undefined2 uVar2;
  
  uVar2 = (undefined2)((ulong)param_1 >> 0x10);
  iVar1 = (int)param_1;
  *(undefined2 *)(iVar1 + 0x50) = param_2;
  *(undefined2 *)(iVar1 + 0x52) = param_3;
  FUN_133d_202c(iVar1,uVar2);
  return;
}



int __stdcall16far FUN_133d_258e(int *param_1)

{
  int *piVar1;
  int *piVar2;
  bool bVar3;
  bool bVar4;
  int iVar5;
  uint uVar6;
  int iVar7;
  undefined2 extraout_DX;
  uint extraout_DX_00;
  uint extraout_DX_01;
  int extraout_DX_02;
  int *piVar8;
  byte *pbVar9;
  int *piVar10;
  int iVar11;
  undefined2 uVar12;
  undefined2 uVar13;
  undefined2 unaff_DS;
  undefined4 uVar14;
  uint local_3a;
  int local_38;
  undefined4 local_32;
  int local_2c;
  int local_24;
  int local_20;
  int iStack_1e;
  undefined1 local_1c;
  undefined1 local_1b;
  undefined2 local_1a;
  undefined4 local_18;
  int local_14;
  uint local_12;
  uint local_10;
  int local_e;
  int local_c;
  int local_a;
  undefined4 local_8;
  int local_4;
  
  local_e = 1;
  iVar7 = *(int *)0x55c;
  local_4 = 0;
  *(undefined2 *)0x568 = 0;
  uVar12 = (undefined2)((ulong)param_1 >> 0x10);
  piVar8 = (int *)param_1;
  if ((*(byte *)(piVar8 + 5) & 0x10) == 0) {
    *(undefined2 *)0x58a = 0;
  }
  else {
    *(undefined2 *)0x58a = 1;
  }
  *(undefined2 *)0x562 = 0;
  FUN_1842_0000();
  while (iVar5 = FUN_1baf_0004(), iVar5 != 0) {
    FUN_1baf_0018();
  }
  if ((*(byte *)(piVar8 + 5) & 4) != 0) {
    for (local_24 = 0; local_24 < piVar8[1]; local_24 = local_24 + 1) {
      FUN_133d_09c8(piVar8,uVar12,local_24 + 1,1 << ((byte)local_24 & 0x1f) & *(uint *)0x554);
    }
  }
  *param_1 = 0;
  local_a = FUN_133d_0f24(piVar8[0x40],piVar8[0x41]);
  local_a = local_a + piVar8[0x23];
  if (-1 < *(int *)0x55c) {
    FUN_133d_0050(piVar8,uVar12);
  }
  if (-1 < *(int *)0x55e) {
    FUN_133d_00d0(piVar8,uVar12);
  }
  if (-1 < *(int *)0x560) {
    FUN_133d_00fa(piVar8,uVar12);
  }
  FUN_133d_0124(piVar8,uVar12);
  if (piVar8[0x35] != 0 || piVar8[0x34] != 0) {
    piVar10 = (int *)piVar8[0x34];
    iVar5 = piVar8[0x35];
    local_18 = (int *)CONCAT22(iVar5,piVar10);
    _local_20 = CONCAT22(piVar10[9],piVar10[8]);
  }
  uVar14 = FUN_133d_14d4(piVar8,uVar12);
  uVar13 = (undefined2)((ulong)uVar14 >> 0x10);
  if ((int)uVar14 != 0) goto LAB_133d_3026;
  if (*(int *)0x56e != 0) {
    FUN_1c4c_0000(200,*(undefined2 *)0x3afc,*(undefined2 *)0x3afe,*(undefined2 *)0x3b00,
                  *(undefined2 *)0x3b02,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                  *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
    uVar13 = extraout_DX;
  }
  uVar14 = CONCAT22(uVar13,local_1a);
  if ((*(byte *)(piVar8 + 5) & 8) == 0) {
    uVar14 = FUN_1c9d_000e(piVar8[0xf],piVar8[0xe],piVar8[0xd],piVar8[0xc]);
  }
  local_3a = (uint)((ulong)uVar14 >> 0x10);
  local_1a = (undefined2)uVar14;
  local_c = 1;
  local_12 = FUN_1d18_0006();
  local_10 = local_3a;
  FUN_1c21_002a();
  if (*(int *)0x564 != 0) {
    uVar13 = *(undefined2 *)0x3c64;
    *(undefined2 *)0x3c64 = 0;
    FUN_1d70_000a(0xfc00,0xa000);
    *(undefined2 *)0x3c64 = uVar13;
    local_3a = extraout_DX_00;
  }
  if ((*(byte *)(piVar8 + 5) & 0x20) != 0) {
    FUN_133d_249c(piVar8,uVar12);
    goto LAB_133d_3026;
  }
  do {
    FUN_1000_0004();
    FUN_1c21_0042();
    bVar4 = false;
    if ((*(int *)0x732 != 0) && (*(int *)0x72c != 0)) {
      if ((*(int *)0x724 < piVar8[8]) ||
         (((*(int *)0x726 < piVar8[9] ||
           (local_3a = piVar8[10] + piVar8[8], (int)local_3a <= *(int *)0x724)) ||
          (piVar8[0xb] + piVar8[9] <= *(int *)0x726)))) {
        local_4 = 1;
        piVar8[0x27] = 0;
        piVar8[0x26] = 0;
        FUN_133d_256c(piVar8,uVar12,0,0);
      }
      else {
        if (piVar8[0x2b] == 0 && piVar8[0x2a] == 0) {
          if (piVar8[0x2f] == 0 && piVar8[0x2e] == 0) goto LAB_133d_28d8;
          local_3a = piVar8[0x2f];
          local_18 = (int *)CONCAT22(local_3a,(int *)piVar8[0x2e]);
          bVar3 = false;
          do {
            if (local_3a == 0 && (int *)local_18 == (int *)0x0) break;
            uVar6 = -(uint)((*(byte *)(piVar8 + 5) & 0x10) == 0) & 3;
            if (((int)(uVar6 + piVar8[9] + *local_18) <= *(int *)0x726) &&
               (*(int *)0x726 < (int)(uVar6 + ((int *)local_18)[1] + piVar8[9]))) {
              bVar3 = true;
              FUN_133d_256c(piVar8,uVar12,(int *)local_18,local_3a);
            }
            uVar13 = (undefined2)((ulong)local_18 >> 0x10);
            local_3a = ((int *)local_18)[9];
            local_18 = (int *)CONCAT22(local_3a,(int *)((int *)local_18)[8]);
          } while (!bVar3);
        }
        else {
          local_3a = piVar8[0x2b];
          local_32 = (byte *)CONCAT22(local_3a,(byte *)piVar8[0x2a]);
          local_14 = piVar8[0x13];
          bVar3 = false;
          do {
            if (local_3a == 0 && (byte *)local_32 == (byte *)0x0) break;
            if (((local_14 + -1 <= *(int *)0x726) && (*(int *)0x726 < local_a + local_14 + -1)) &&
               ((*local_32 & 1) == 0)) {
              piVar8[0x26] = (int)(byte *)local_32;
              piVar8[0x27] = local_3a;
              bVar3 = true;
              local_4 = 1;
            }
            local_14 = local_14 + local_a;
            uVar13 = (undefined2)((ulong)local_32 >> 0x10);
            local_3a = *(uint *)((byte *)local_32 + 0x12);
            local_32 = (byte *)CONCAT22(local_3a,*(byte **)((byte *)local_32 + 0x10));
          } while (!bVar3);
        }
        if (!bVar3) {
          bVar4 = true;
        }
      }
    }
LAB_133d_28d8:
    if ((local_c == 0) || (iVar5 = FUN_1baf_0004(), iVar5 == 0)) goto LAB_133d_2bb2;
    local_38 = FUN_1baf_0018();
    if (piVar8[0x31] != 0 || piVar8[0x30] != 0) {
      pbVar9 = (byte *)piVar8[0x30];
      local_3a = piVar8[0x31];
      local_8 = (byte *)CONCAT22(local_3a,pbVar9);
      if (local_38 == 8) {
        iVar5 = FUN_2388_0dd4(*(undefined2 *)(pbVar9 + 0xc),*(undefined2 *)(pbVar9 + 0xe));
        if (iVar5 != 0) {
          uVar13 = (undefined2)((ulong)local_8 >> 0x10);
          iVar5 = FUN_2388_0dd4(*(undefined2 *)((byte *)local_8 + 0xc),
                                *(undefined2 *)((byte *)local_8 + 0xe));
          *(undefined1 *)((int)*(undefined4 *)((byte *)local_8 + 0xc) + iVar5 + -1) = 0;
        }
LAB_133d_29fb:
        *local_8 = *local_8 & 0x7f;
LAB_133d_2a02:
        FUN_133d_1e22(piVar8,uVar12);
        local_3a = extraout_DX_01;
        goto LAB_133d_2bb2;
      }
      if (local_38 == 0xd) goto LAB_133d_2bad;
      if (local_38 == 0x1b) goto LAB_133d_2ba8;
      if (local_38 == 0x13b) {
        if (*(int *)0x566 == 0) goto LAB_133d_2bb2;
LAB_133d_29b7:
        local_c = 0;
        *(undefined2 *)0x568 = 1;
      }
      else if (local_38 == 0x153) {
        if ((*local_8 & 0x80) != 0) {
          **(undefined1 **)(pbVar9 + 0xc) = 0;
          goto LAB_133d_29fb;
        }
      }
      else if ((local_38 < 0x100) && ((*(byte *)(local_38 + 0x45a9) & 0x57) != 0)) {
        if ((*local_8 & 0x80) != 0) {
          **(undefined1 **)(pbVar9 + 0xc) = 0;
          *local_8 = *local_8 & 0x7f;
        }
        local_1c = (undefined1)local_38;
        local_1b = 0;
        uVar6 = FUN_2388_0dd4(*(undefined2 *)(pbVar9 + 0xc),*(undefined2 *)(pbVar9 + 0xe));
        uVar13 = (undefined2)((ulong)local_8 >> 0x10);
        pbVar9 = (byte *)local_8;
        if (uVar6 < *(uint *)(pbVar9 + 6)) {
          FUN_2388_0e22(*(undefined2 *)(pbVar9 + 0xc),*(undefined2 *)(pbVar9 + 0xe),&local_1c);
        }
        goto LAB_133d_2a02;
      }
      goto LAB_133d_2bb2;
    }
    if ((local_38 < 0x100) && ((*(byte *)(local_38 + 0x45a9) & 2) != 0)) {
      local_38 = local_38 + -0x20;
    }
    if (piVar8[0x2b] == 0 && piVar8[0x2a] == 0) {
      if (local_38 == 0x1b) {
LAB_133d_2ba8:
        *param_1 = -1;
LAB_133d_2bad:
        local_c = 0;
        goto LAB_133d_2bb2;
      }
      if (local_38 == 0x148) {
        *(byte *)(piVar8 + 5) = *(byte *)(piVar8 + 5) | 0x80;
        if (piVar8[0x2f] == 0 && piVar8[0x2e] == 0) goto LAB_133d_2bb2;
        if ((piVar8[0x29] == 0 && piVar8[0x28] == 0) ||
           ((piVar8[0x2e] == piVar8[0x28] && (piVar8[0x2f] == piVar8[0x29])))) {
          local_3a = piVar8[0x2f];
          piVar10 = (int *)piVar8[0x2e];
          while (local_18 = (int *)CONCAT22(local_3a,piVar10), piVar10[9] != 0 || piVar10[8] != 0) {
            piVar1 = piVar10 + 8;
            local_3a = piVar10[9];
            piVar10 = (int *)*piVar1;
          }
        }
        else {
          local_3a = piVar8[0x2f];
          piVar10 = (int *)piVar8[0x2e];
          do {
            do {
              local_18 = (int *)CONCAT22(local_3a,piVar10);
              piVar2 = (int *)piVar10[8];
              local_3a = piVar10[9];
              piVar10 = piVar2;
            } while ((int *)piVar8[0x28] != piVar2);
          } while (piVar8[0x29] != local_3a);
        }
        piVar10 = (int *)local_18;
        uVar13 = local_18._2_2_;
      }
      else {
        if (local_38 != 0x150) {
          *(byte *)(piVar8 + 5) = *(byte *)(piVar8 + 5) | 0x80;
          if ((piVar8[0x29] != 0 || piVar8[0x28] != 0) && ((*(byte *)(piVar8 + 5) & 2) != 0)) {
            uVar13 = (undefined2)((ulong)*(int **)(piVar8 + 0x28) >> 0x10);
            piVar10 = (int *)*(int **)(piVar8 + 0x28);
            if (*(char *)(piVar10[2] * 0x1c + 0x5226) != '\0') {
              *(undefined1 *)(piVar10[2] * 0x1c + 0x5226) = 0;
              goto LAB_133d_2aec;
            }
          }
          goto LAB_133d_2bad;
        }
        *(byte *)(piVar8 + 5) = *(byte *)(piVar8 + 5) | 0x80;
        if (piVar8[0x2f] == 0 && piVar8[0x2e] == 0) goto LAB_133d_2bb2;
        if (piVar8[0x29] == 0 && piVar8[0x28] == 0) {
LAB_133d_2ada:
          piVar10 = (int *)piVar8[0x2e];
          local_3a = piVar8[0x2f];
          local_18 = (int *)CONCAT22(local_3a,piVar10);
        }
        else {
          uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x28) >> 0x10);
          iVar5 = (int)*(undefined4 *)(piVar8 + 0x28);
          piVar10 = (int *)*(undefined2 *)(iVar5 + 0x10);
          local_3a = *(uint *)(iVar5 + 0x12);
          local_18 = (int *)CONCAT22(local_3a,piVar10);
          local_3a = local_3a | (uint)piVar10;
          if (local_3a == 0) goto LAB_133d_2ada;
        }
        uVar13 = local_18._2_2_;
      }
LAB_133d_2aec:
      FUN_133d_256c(piVar8,uVar12,piVar10,uVar13);
      goto LAB_133d_2bb2;
    }
    if (local_38 == 0x20) {
LAB_133d_2d0a:
      if (piVar8[0x27] != 0 || piVar8[0x26] != 0) {
        if ((*(byte *)(piVar8 + 5) & 4) != 0) {
          if (local_38 == 0xd) {
            local_c = 0;
          }
          else {
            uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x26) >> 0x10);
            iVar5 = (int)*(undefined4 *)(piVar8 + 0x26);
            *(uint *)(iVar5 + 6) = (uint)(*(int *)(iVar5 + 6) == 0);
          }
          goto LAB_133d_2d43;
        }
        if ((local_38 != 0x13b) || (*(int *)0x566 != 0)) {
          *param_1 = *(int *)((int)*(undefined4 *)(piVar8 + 0x26) + 4);
          local_c = 0;
          if (local_38 == 0x13b) goto LAB_133d_29b7;
        }
      }
    }
    else {
      if (local_38 < 0x21) {
        if (local_38 != 0xd) {
          if (local_38 != 0x1b) {
LAB_133d_2e04:
            bVar3 = false;
            local_3a = piVar8[0x2b];
            iVar5 = piVar8[0x2a];
            while ((!bVar3 && (local_3a != 0 || iVar5 != 0))) {
              if (*(int *)(iVar5 + 2) == local_38) {
                bVar3 = true;
              }
              else {
                piVar1 = (int *)(iVar5 + 0x10);
                local_3a = *(uint *)(iVar5 + 0x12);
                iVar5 = *piVar1;
              }
            }
            if (bVar3) {
              piVar8[0x26] = iVar5;
              piVar8[0x27] = local_3a;
              local_4 = 1;
              if ((*(byte *)(piVar8 + 5) & 4) == 0) goto LAB_133d_2bad;
              *(uint *)(iVar5 + 6) = (uint)(*(int *)(iVar5 + 6) == 0);
            }
            goto LAB_133d_2bb2;
          }
          goto LAB_133d_2ba8;
        }
        goto LAB_133d_2d0a;
      }
      if (local_38 == 0x13b) goto LAB_133d_2d0a;
      if (local_38 == 0x148) {
        local_2c = 0;
        do {
          if (piVar8[0x27] != 0 || piVar8[0x26] != 0) {
            uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x26) >> 0x10);
            iVar5 = (int)*(undefined4 *)(piVar8 + 0x26);
            local_3a = *(uint *)(iVar5 + 0x16);
            piVar8[0x26] = *(int *)(iVar5 + 0x14);
            piVar8[0x27] = local_3a;
          }
          if (piVar8[0x27] == 0 && piVar8[0x26] == 0) {
            local_3a = piVar8[0x39];
            piVar8[0x26] = piVar8[0x38];
            piVar8[0x27] = local_3a;
            local_2c = local_2c + 1;
          }
        } while (((**(byte **)(piVar8 + 0x26) & 1) != 0) && (local_2c < 2));
      }
      else {
        if (local_38 != 0x150) goto LAB_133d_2e04;
        local_2c = 0;
        do {
          if (piVar8[0x27] != 0 || piVar8[0x26] != 0) {
            uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x26) >> 0x10);
            iVar5 = (int)*(undefined4 *)(piVar8 + 0x26);
            local_3a = *(uint *)(iVar5 + 0x12);
            piVar8[0x26] = *(int *)(iVar5 + 0x10);
            piVar8[0x27] = local_3a;
          }
          if (piVar8[0x27] == 0 && piVar8[0x26] == 0) {
            local_3a = piVar8[0x2b];
            piVar8[0x26] = piVar8[0x2a];
            piVar8[0x27] = local_3a;
            local_2c = local_2c + 1;
          }
        } while (((**(byte **)(piVar8 + 0x26) & 1) != 0) && (local_2c < 2));
      }
LAB_133d_2d43:
      local_4 = 1;
    }
LAB_133d_2bb2:
    if (*(int *)0x56e != 0) {
      if (iStack_1e != 0 || local_20 != 0) {
        uVar6 = FUN_1d18_0006();
        if ((*(int *)0x4e88 <= (int)local_3a) &&
           (((*(int *)0x4e88 < (int)local_3a || (*(uint *)0x4e86 <= uVar6)) &&
            (*(int *)0x4e7e < *(int *)((int)*(undefined4 *)(local_20 + 0xc) + 4))))) {
          *(int *)0x4e7e = *(int *)0x4e7e + 1;
          FUN_1c4c_0000(200,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                        *(undefined2 *)0x3afa,*(undefined2 *)0x3afc,*(undefined2 *)0x3afe,
                        *(undefined2 *)0x3b00,*(undefined2 *)0x3b02);
          local_e = 1;
          iVar5 = extraout_DX_02;
          uVar6 = FUN_1d18_0006();
          *(int *)0x4e86 = uVar6 + 10;
          *(int *)0x4e88 = iVar5 + (uint)(0xfff5 < uVar6);
        }
      }
    }
    if (local_e == 0) {
      if (local_4 != 0) {
        FUN_133d_1b8a(piVar8,uVar12);
      }
    }
    else {
      FUN_133d_249c(piVar8,uVar12);
    }
    local_e = 0;
    local_4 = 0;
    if ((*(int *)0x730 != 0) && (!bVar4)) {
      if (piVar8[0x2b] == 0 && piVar8[0x2a] == 0) {
        if (((piVar8[0x2f] != 0 || piVar8[0x2e] != 0) && (piVar8[0x29] != 0 || piVar8[0x28] != 0))
           && ((*(byte *)(piVar8 + 5) & 2) != 0)) {
          uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x28) >> 0x10);
          iVar11 = (int)*(undefined4 *)(piVar8 + 0x28);
          iVar5 = *(int *)(iVar11 + 4) * 0x1c;
          if (*(char *)(iVar5 + 0x5226) != '\0') {
            *(undefined1 *)(iVar5 + 0x5226) = 0;
            FUN_133d_256c(piVar8,uVar12,iVar11,uVar13);
            goto LAB_133d_2e66;
          }
        }
LAB_133d_2e61:
        local_c = 0;
      }
      else if ((piVar8[0x27] != 0 || piVar8[0x26] != 0) || ((*(byte *)(piVar8 + 5) & 1) != 0)) {
        if ((piVar8[0x27] == 0 && piVar8[0x26] == 0) || ((*(byte *)(piVar8 + 5) & 4) == 0))
        goto LAB_133d_2e61;
        uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x26) >> 0x10);
        iVar5 = (int)*(undefined4 *)(piVar8 + 0x26);
        *(uint *)(iVar5 + 6) = (uint)(*(int *)(iVar5 + 6) == 0);
        local_4 = 1;
      }
    }
LAB_133d_2e66:
    if ((((*(int *)0x730 != 0) && (local_c == 0)) && (*(int *)0x566 != 0)) && (*(int *)0x720 != 0))
    {
      *(undefined2 *)0x568 = 1;
    }
    if ((local_c == 0) || (*(int *)0x72a == 0)) {
      local_3a = 0;
    }
    else {
      local_3a = 1;
    }
    FUN_1c21_0110();
    if (*(int *)0x8a != 0) {
      uVar6 = FUN_1d18_0006();
      iVar5 = local_10 + (0xff87 < local_12);
      if ((iVar5 <= (int)local_3a) && ((iVar5 < (int)local_3a || (local_12 + 0x78 <= uVar6)))) {
        local_c = 0;
      }
    }
  } while (local_c != 0);
  if (piVar8[0x31] == 0 && piVar8[0x30] == 0) {
    if (*param_1 == 0) {
      if (piVar8[0x27] == 0 && piVar8[0x26] == 0) {
        if (piVar8[0x29] == 0 && piVar8[0x28] == 0) goto LAB_133d_2fa4;
        iVar5 = *(int *)((int)*(undefined4 *)(piVar8 + 0x28) + 6);
      }
      else {
        iVar5 = *(int *)((int)*(undefined4 *)(piVar8 + 0x26) + 4);
      }
      *param_1 = iVar5;
    }
  }
  else {
    FUN_2388_0dec(0x4b64);
  }
LAB_133d_2fa4:
  if ((*(byte *)(piVar8 + 5) & 8) == 0) {
    FUN_1c9d_00ec(piVar8[0xf],piVar8[0xe],piVar8[0xd],piVar8[0xc]);
  }
  if (piVar8[0x35] != 0 || piVar8[0x34] != 0) {
    if (*(int *)0x570 != 1) {
      uVar13 = (undefined2)((ulong)*(undefined4 *)(piVar8 + 0x34) >> 0x10);
      iVar5 = (int)*(undefined4 *)(piVar8 + 0x34);
      FUN_1cc9_0310(*(undefined2 *)(iVar5 + 0xc),*(undefined2 *)(iVar5 + 0xe));
    }
    if ((7 < iVar7) && (*(int *)0x570 == 0)) {
      local_18 = (int *)*(undefined4 *)((int)*(undefined4 *)(piVar8 + 0x34) + 0x10);
      uVar13 = (undefined2)((ulong)local_18 >> 0x10);
      FUN_1cc9_0310(((int *)local_18)[6],((int *)local_18)[7]);
    }
  }
  FUN_133d_1a4a(piVar8,uVar12);
LAB_133d_3026:
  *(undefined2 *)0x55c = 0xffff;
  *(undefined2 *)0x55e = 0xffff;
  *(undefined2 *)0x560 = 0xffff;
  *(undefined2 *)0x58a = 0;
  *(undefined2 *)0x566 = 0;
  if ((*(byte *)(piVar8 + 5) & 4) != 0) {
    *(undefined2 *)0x554 = 0;
    for (local_24 = 0; local_24 < piVar8[1]; local_24 = local_24 + 1) {
      iVar7 = FUN_133d_0998(piVar8,uVar12,local_24 + 1);
      if (iVar7 != 0) {
        *(uint *)0x554 = *(uint *)0x554 | 1 << ((byte)local_24 & 0x1f);
      }
    }
  }
  FUN_1c21_002a();
  if (*(int *)0x570 != 0) {
    FUN_1000_0002();
  }
  return *param_1;
}



void __cdecl16far FUN_133d_3092(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x59e = param_1;
  *(undefined2 *)0x5a0 = param_2;
  *(undefined2 *)0x5a2 = param_3;
  return;
}



void __cdecl16far FUN_133d_30aa(char *param_1,undefined1 *param_2)

{
  undefined1 *puVar1;
  int iVar2;
  undefined2 unaff_DS;
  undefined1 *puVar3;
  undefined2 uVar4;
  char *local_2e;
  uint local_2c;
  undefined1 local_2a [40];
  
  *param_2 = 0;
  do {
    puVar1 = (undefined1 *)FUN_2388_09e8(param_1,0x25);
    if (puVar1 != (undefined1 *)0x0) {
      *puVar1 = 0;
    }
    if (*param_1 != '\0') {
      FUN_2388_05e6(param_2,param_1);
    }
    if (puVar1 == (undefined1 *)0x0) {
      local_2e = (char *)0x0;
    }
    else {
      local_2e = puVar1 + 1;
      iVar2 = FUN_2388_0a12(local_2e,0x5a4,6);
      if (iVar2 == 0) {
        iVar2 = thunk_FUN_2388_1e76(puVar1 + 7);
        FUN_2388_05e6(param_2,iVar2 * 0x40 + 0x634e,iVar2);
        param_1 = puVar1 + 8;
      }
      else {
        iVar2 = FUN_2388_06fe(local_2e,0x5ab,6);
        if (iVar2 == 0) {
          uVar4 = 10;
          puVar3 = local_2a;
          iVar2 = thunk_FUN_2388_1e76(puVar1 + 7,puVar3,10);
          FUN_2388_0758(0x2388,*(undefined2 *)(iVar2 * 4 + 0x698a),
                        *(undefined2 *)(iVar2 * 4 + 0x698c),puVar3,uVar4,iVar2);
          FUN_2388_05e6(param_2,local_2a);
LAB_133d_3186:
          param_1 = puVar1 + 8;
        }
        else {
          iVar2 = FUN_2388_06fe(local_2e,0x5b2,3);
          if (iVar2 == 0) {
            uVar4 = 0x10;
            puVar3 = local_2a;
            iVar2 = thunk_FUN_2388_1e76(puVar1 + 4,puVar3,0x10);
            FUN_2388_0758(0x2388,*(undefined2 *)(iVar2 * 4 + 0x698a),
                          *(undefined2 *)(iVar2 * 4 + 0x698c),puVar3,uVar4,iVar2);
            for (local_2c = 0; iVar2 = FUN_2388_0684(local_2a),
                local_2c <= (uint)-(iVar2 + -4) && -local_2c != iVar2 + -4; local_2c = local_2c + 1)
            {
              FUN_2388_05e6(param_2,0x5b6);
            }
            FUN_2388_05e6(param_2,local_2a);
          }
          else {
            iVar2 = FUN_2388_06fe(local_2e,0x5b8,7);
            if (iVar2 == 0) {
              local_2a[0] = 0;
              FUN_1000_0014(*(undefined2 *)0x5e44,0,local_2a);
              FUN_2388_0e22(param_2);
              goto LAB_133d_3186;
            }
            iVar2 = FUN_2388_06fe(local_2e,0x5c0,4);
            if (iVar2 != 0) {
              param_1 = local_2e;
              if (*local_2e == '%') {
                FUN_2388_0e22(param_2);
                param_1 = puVar1 + 2;
              }
              goto LAB_133d_32a7;
            }
            FUN_2388_073c(*(undefined2 *)0x5e36,local_2a,10);
            FUN_2388_0e22(param_2);
          }
          param_1 = puVar1 + 5;
        }
      }
    }
LAB_133d_32a7:
    if (local_2e == (char *)0x0) {
      return;
    }
  } while( true );
}



uint __cdecl16far FUN_133d_32b2(void)

{
  int iVar1;
  undefined2 uVar2;
  uint in_DX;
  uint uVar3;
  uint uVar4;
  undefined2 unaff_DS;
  uint local_16c;
  char *local_16a;
  uint local_164;
  undefined1 local_162 [80];
  undefined1 local_112 [256];
  int local_12;
  char *local_10;
  uint local_e;
  uint uStack_c;
  undefined2 local_a;
  uint local_8;
  int local_6;
  undefined2 local_4;
  
  local_6 = 1;
  local_164 = 1;
  local_12 = 0;
  local_e = 0;
  uStack_c = 0;
  uVar3 = in_DX;
  FUN_2388_0626(0x3b32);
  iVar1 = FUN_1842_001a();
  if (iVar1 == 0) {
    local_e = FUN_133d_06de(*(undefined2 *)0x5a2,*(undefined2 *)0x59e,*(undefined2 *)0x5a0);
    uVar4 = uVar3 | local_e;
    uStack_c = uVar3;
    if (uVar4 != 0) {
      local_16c = in_DX;
      if ((*(int *)0x608 != 0) && (*(int *)0x5e2a != 0)) {
        FUN_2388_0626(local_162,*(undefined2 *)0x5e2a);
      }
      do {
        local_10 = (char *)FUN_1842_0106();
        iVar1 = FUN_2388_0684(local_10);
        if (iVar1 == 0) {
          local_6 = local_6 + 1;
        }
        else if (*local_10 == '@') {
          FUN_2388_0a88(local_10);
          local_16a = local_10 + 1;
          iVar1 = FUN_2388_0658(local_16a,0x5c7);
          if ((iVar1 == 0) || (iVar1 = FUN_2388_0658(local_16a,0x5cf), iVar1 == 0)) {
            local_6 = 2;
          }
          else {
            iVar1 = FUN_2388_0658(local_16a,0x5d6);
            if (iVar1 == 0) {
              local_6 = 1;
            }
            else {
              iVar1 = FUN_2388_0658(local_16a,0x5db);
              if (iVar1 == 0) {
                uVar4 = *(uint *)0x82;
                *(undefined2 *)(local_e + 0x80) = *(undefined2 *)0x80;
                *(uint *)(local_e + 0x82) = uVar4;
              }
              else {
                iVar1 = FUN_2388_06fe(local_16a,0x5e5,1);
                if (iVar1 == 0) {
                  while ((*local_16a != '\0' && ((*(byte *)(*local_16a + 0x45a9) & 4) == 0))) {
                    local_16a = local_16a + 1;
                  }
                  uVar2 = thunk_FUN_2388_1e76(local_16a);
                  *(undefined2 *)(local_e + 0xe) = uVar2;
                }
                else {
                  iVar1 = FUN_2388_06fe(local_16a,0x5e7,1);
                  if (iVar1 == 0) {
                    while ((*local_16a != '\0' && ((*(byte *)(*local_16a + 0x45a9) & 4) == 0))) {
                      local_16a = local_16a + 1;
                    }
                    uVar2 = thunk_FUN_2388_1e76(local_16a);
                    *(undefined2 *)(local_e + 0xc) = uVar2;
                  }
                  else {
                    iVar1 = FUN_2388_06fe(local_16a,0x5e9,5);
                    if (iVar1 == 0) {
                      while ((*local_16a != '\0' && ((*(byte *)(*local_16a + 0x45a9) & 4) == 0))) {
                        local_16a = local_16a + 1;
                      }
                      local_4 = thunk_FUN_2388_1e76(local_16a);
                      FUN_133d_0c30(local_e,uStack_c,local_4);
                    }
                    else {
                      iVar1 = FUN_2388_0a12(local_16a,0x5ef,6);
                      if (iVar1 == 0) {
                        while ((*local_16a != '\0' && ((*(byte *)(*local_16a + 0x45a9) & 4) == 0)))
                        {
                          local_16a = local_16a + 1;
                        }
                        if (*(int *)0x5ad0 == 0) {
                          uVar2 = thunk_FUN_2388_1e76(local_16a);
                          *(undefined2 *)0x5ad0 = uVar2;
                        }
                      }
                      else {
                        iVar1 = FUN_2388_06fe(local_16a,0x5f6,7);
                        if (iVar1 == 0) {
                          local_12 = 1;
                          *(byte *)(local_e + 10) = *(byte *)(local_e + 10) | 5;
                        }
                        else {
                          iVar1 = FUN_2388_06fe(local_16a,0x5ff,7);
                          if (iVar1 == 0) {
                            if (*(int *)0x608 == 0) {
                              while ((*local_16a != '\0' &&
                                     ((*(byte *)(*local_16a + 0x45a9) & 4) == 0))) {
                                local_16a = local_16a + 1;
                              }
                              if (local_16c == 0) {
                                local_16c = thunk_FUN_2388_1e76(local_16a);
                              }
                            }
                            else {
                              for (; (*local_16a != '\0' && (*local_16a != '='));
                                  local_16a = local_16a + 1) {
                              }
                              if (*(int *)0x5e2a == 0) {
                                if (*local_16a != '\0') {
                                  local_16a = local_16a + 1;
                                }
                                FUN_2388_0626(local_162,local_16a);
                              }
                            }
                          }
                          else {
                            local_6 = 3;
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        else if (local_6 == 1) {
          FUN_133d_30aa(local_10,local_112);
          FUN_133d_0c40(local_e,uStack_c,local_112);
        }
        else if (local_6 == 2) {
          if (*(int *)0x608 == 0) {
            FUN_133d_30aa(local_10,local_112);
            iVar1 = FUN_133d_0a0e(local_e,uStack_c,local_112);
            if (local_12 != 0) {
              *(undefined2 *)(iVar1 + 6) = 0;
            }
            if (local_164 == local_16c) {
              *(int *)(local_e + 0x4c) = iVar1;
              *(uint *)(local_e + 0x4e) = uVar4;
            }
            local_164 = local_164 + 1;
          }
          else {
            if (*(int *)0x5ad0 == 0) {
              *(undefined2 *)0x5ad0 = 5;
            }
            FUN_133d_30aa(local_10,local_112);
            local_a = FUN_133d_0d52(local_e,uStack_c,local_112);
            local_8 = uVar4;
          }
        }
      } while (local_6 < 3);
    }
  }
  if (uStack_c == 0 && local_e == 0) {
    *(undefined2 *)0x55e = 0xffff;
    *(undefined2 *)0x55c = 0xffff;
    *(undefined2 *)0x560 = 0xffff;
    *(undefined2 *)0x566 = 0;
  }
  return local_e;
}



undefined2 __cdecl16far FUN_133d_36d8(void)

{
  int iVar1;
  int in_DX;
  undefined2 local_4;
  
  local_4 = 0;
  iVar1 = FUN_133d_32b2();
  if (in_DX != 0 || iVar1 != 0) {
    local_4 = FUN_133d_258e(iVar1,in_DX);
    FUN_1cc9_0310(iVar1,in_DX);
  }
  return local_4;
}



void __cdecl16far FUN_133d_3712(void)

{
  char in_AL;
  int in_DX;
  undefined2 unaff_DS;
  
  if (in_DX != 0) {
    *(uint *)0x554 = *(uint *)0x554 | 1 << (in_AL - 1U & 0x1f);
    return;
  }
  *(uint *)0x554 = *(uint *)0x554 & ~(1 << (in_AL - 1U & 0x1f));
  return;
}



uint __cdecl16far FUN_133d_373c(void)

{
  char in_AL;
  undefined2 unaff_DS;
  
  return 1 << (in_AL - 1U & 0x1f) & *(uint *)0x554;
}



void __cdecl16far FUN_133d_376e(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x55c = param_2;
  FUN_133d_36d8();
  return;
}



void __cdecl16far FUN_133d_3798(void)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x55c = 8;
  FUN_133d_36d8();
  return;
}



void __cdecl16far FUN_133d_37b0(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x55e = param_2;
  FUN_133d_36d8();
  return;
}



void __cdecl16far FUN_133d_37da(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x560 = param_2;
  FUN_133d_36d8();
  return;
}



undefined2 __stdcall16far FUN_133d_380a(undefined2 param_1)

{
  int iVar1;
  undefined2 in_DX;
  int iVar2;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  *(undefined2 *)0x608 = 1;
  *(undefined2 *)0x5ad0 = param_1;
  *(undefined2 *)0x5e2a = in_DX;
  iVar2 = 0;
  local_4 = 0;
  iVar1 = FUN_133d_32b2();
  if (iVar2 != 0 || iVar1 != 0) {
    local_4 = FUN_133d_258e(iVar1,iVar2);
    FUN_1cc9_0310(iVar1,iVar2);
  }
  *(undefined2 *)0x608 = 0;
  return local_4;
}



undefined2 __cdecl16far FUN_133d_3856(void)

{
  undefined2 uVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  
  FUN_2388_073c();
  uVar1 = FUN_133d_380a(5);
  uVar2 = thunk_FUN_2388_1e76(0x4b64);
  *(undefined2 *)0x5ad4 = uVar2;
  return uVar1;
}



void __cdecl16far FUN_133d_3898(void)

{
  int in_DX;
  undefined2 unaff_DS;
  byte local_8 [2];
  int local_6;
  int local_4;
  
  local_6 = FUN_1ddb_0008(1,1,local_8);
  if (in_DX != 0 || local_6 != 0) {
    local_4 = in_DX;
    FUN_1d8f_0000(0,local_6,in_DX);
    *(uint *)0x53c = (uint)local_8[0];
    FUN_1d8f_0000(0,local_6,local_4);
    *(uint *)0x53e = (uint)local_8[0];
    FUN_1d8f_0000(0,local_6,local_4);
    *(uint *)0x540 = (uint)local_8[0];
    FUN_1d8f_0000(0,local_6,local_4);
    *(uint *)0x542 = (uint)local_8[0];
    FUN_1d8f_0000(0,local_6,local_4);
    *(uint *)0x54a = (uint)local_8[0];
    FUN_1d8f_0000(0,local_6,local_4);
    *(uint *)0x54e = (uint)local_8[0];
    FUN_1d8f_0000(0,local_6,local_4);
    *(uint *)0x54c = (uint)local_8[0];
    FUN_1d70_000a(0xfc00,0xa000);
    FUN_1cc9_0310(local_6,local_4);
  }
  return;
}



char __cdecl16near FUN_16d7_0008(char *param_1)

{
  char cVar1;
  
  cVar1 = *param_1;
  if (cVar1 == '\x06') {
    cVar1 = '\x05';
  }
  return cVar1;
}



void __cdecl16far
FUN_16d7_001e(undefined2 *param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5,undefined2 param_6,undefined2 param_7,undefined2 param_8)

{
  *param_1 = param_5;
  param_1[1] = param_6;
  param_1[2] = param_7;
  param_1[3] = param_8;
  param_1[4] = param_3;
  param_1[5] = param_4;
  return;
}



void FUN_16d7_0052(void)

{
  undefined2 *puVar1;
  undefined2 unaff_DS;
  char in_stack_00000008;
  undefined2 in_stack_00000010;
  undefined2 in_stack_00000012;
  undefined2 in_stack_00000014;
  undefined2 in_stack_00000016;
  undefined2 in_stack_00000018;
  
  if ((*(int *)0x632 != 0) && (in_stack_00000008 == '\a')) {
    puVar1 = (undefined2 *)*(int *)0x632;
    FUN_1cb9_0000(in_stack_00000012,in_stack_00000014,in_stack_00000016,in_stack_00000018,*puVar1,
                  puVar1[1],puVar1[2],puVar1[3]);
    return;
  }
  FUN_1c5b_0004(in_stack_00000008,in_stack_00000010,in_stack_00000012,in_stack_00000014,
                in_stack_00000016,in_stack_00000018);
  return;
}



void __cdecl16far FUN_16d7_00b6(undefined4 param_1)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_SS;
  undefined1 local_52 [80];
  
  FUN_2388_0dec(local_52);
  iVar1 = FUN_2388_09e8(local_52,0x7e);
  if (iVar1 != 0) {
    FUN_2388_0dec(iVar1);
  }
  uVar2 = (undefined2)((ulong)param_1 >> 0x10);
  FUN_1d6c_0002(local_52,unaff_SS,*(undefined2 *)((int)param_1 + 8),
                *(undefined2 *)((int)param_1 + 10));
  return;
}



int __cdecl16far
FUN_16d7_010c(int *param_1,undefined2 param_2,char *param_3,int param_4,undefined2 param_5,
             int param_6)

{
  char cVar1;
  undefined2 unaff_SS;
  undefined4 local_10;
  undefined4 local_c;
  char local_8;
  undefined1 local_7;
  undefined2 local_4;
  
  if (param_6 == 0) {
    local_7 = 0;
    FUN_1d6a_0006(param_1[1]);
    cVar1 = *param_3;
    local_4 = param_3._2_2_;
    while (cVar1 != '\0') {
      if (*(char *)param_3 == '~') {
        local_c = (int *)CONCAT22(param_2,param_1);
        FUN_1d6a_0006(param_1[3]);
        param_3._0_2_ = (char *)param_3 + 1;
        local_8 = *(char *)param_3;
        param_4 = FUN_1d53_0008(*local_c,&local_8,unaff_SS,param_1[4],param_1[5]);
        param_4 = param_4 + *local_c;
        FUN_1d6a_0006(param_1[1]);
      }
      else {
        local_8 = *(char *)param_3;
        local_10 = (int *)CONCAT22(param_2,param_1);
        param_4 = FUN_1d53_0008(*param_1,&local_8,unaff_SS,param_1[4],param_1[5]);
        param_4 = param_4 + *local_10;
      }
      param_3._0_2_ = (char *)param_3 + 1;
      cVar1 = *(char *)param_3;
    }
  }
  else {
    FUN_1d6a_0006(param_1[2]);
    param_4 = FUN_1d53_0008(*param_1,(char *)param_3,param_3._2_2_,param_1[4],param_1[5]);
    param_4 = param_4 + *param_1;
  }
  return param_4;
}



uint __cdecl16far FUN_16d7_0246(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  int in_DX;
  int iVar5;
  undefined2 unaff_SS;
  
  iVar1 = FUN_2388_0d82(param_1,param_2,0x7e);
  iVar5 = in_DX;
  iVar2 = FUN_2388_0ca8(param_1,param_2,0x7e);
  if ((((iVar2 == iVar1) && (iVar5 == in_DX)) || (*(char *)(iVar2 + 1) != 'F')) ||
     ((*(byte *)(*(byte *)(iVar1 + 1) + 0x45a9) & 4) == 0)) {
    if (in_DX == 0 && iVar1 == 0) {
      uVar4 = 0;
    }
    else {
      uVar4 = (uint)*(byte *)(iVar1 + 1);
      if ((*(byte *)(uVar4 + 0x45a9) & 2) != 0) {
        uVar4 = uVar4 - 0x20;
      }
    }
  }
  else {
    iVar3 = 0x13b;
    if ((*(char *)(iVar2 + 3) == '0') && (iVar3 = 0x154, *(char *)(iVar2 + 5) == '0')) {
      iVar3 = 0x15e;
    }
    if (*(char *)(iVar1 + -1) == '1') {
      uVar4 = iVar3 + 9;
    }
    else {
      uVar4 = iVar3 + (*(byte *)(iVar1 + 1) - 0x31);
    }
  }
  return uVar4;
}



// WARNING: Removing unreachable block (ram,0x000171a5)

int __cdecl16far FUN_16d7_02fe(int param_1,undefined2 param_2,undefined2 param_3)

{
  int iVar1;
  int iVar2;
  undefined2 unaff_DS;
  undefined2 local_e;
  
  local_e = 0;
  iVar2 = param_1 + 0x4e >> 0xf;
  iVar1 = FUN_1cc9_02e2();
  if (iVar2 != 0 || iVar1 != 0) {
    FUN_1f45_0090(param_1,param_1 >> 0xf,iVar1 + 0x4e,iVar2,iVar1 + 0x3c,iVar2);
    *(undefined2 *)(iVar1 + 0x3a) = 0;
    *(undefined2 *)(iVar1 + 0x38) = 0;
    *(undefined2 *)(iVar1 + 2) = 0;
    *(undefined2 *)(iVar1 + 6) = 0xc;
    *(undefined2 *)(iVar1 + 8) = 3;
    *(undefined2 *)(iVar1 + 0xc) = 4;
    *(undefined2 *)(iVar1 + 4) = 1;
    *(undefined2 *)(iVar1 + 10) = 1;
    *(undefined2 *)(iVar1 + 0xe) = *(undefined2 *)0x614;
    *(undefined2 *)(iVar1 + 0x10) = *(undefined2 *)0x616;
    FUN_16d7_001e(iVar1 + 0x20,iVar2,param_2,param_3,0,*(undefined2 *)0x62c,*(undefined2 *)0x62e,
                  *(undefined2 *)0x630);
    *(undefined2 *)(iVar1 + 0x1a) = *(undefined2 *)0x620;
    *(undefined2 *)(iVar1 + 0x1c) = *(undefined2 *)0x622;
    *(undefined2 *)(iVar1 + 0x12) = *(undefined2 *)0x618;
    *(undefined2 *)(iVar1 + 0x14) = *(undefined2 *)0x61a;
    *(undefined2 *)(iVar1 + 0x16) = *(undefined2 *)0x61c;
    *(undefined2 *)(iVar1 + 0x18) = *(undefined2 *)0x61e;
    *(undefined2 *)(iVar1 + 0x1e) = *(undefined2 *)0x624;
    FUN_16d7_001e(iVar1 + 0x2c,iVar2,param_2,param_3,0,*(undefined2 *)0x626,*(undefined2 *)0x628,
                  *(undefined2 *)0x62a);
    local_e = iVar1;
  }
  return local_e;
}



int __cdecl16far FUN_16d7_0446(undefined4 param_1,int param_2)

{
  undefined4 *puVar1;
  undefined4 uVar2;
  bool bVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  
  bVar3 = false;
  iVar5 = 0;
  uVar2 = *(undefined4 *)((int)param_1 + 0x38);
  iVar6 = (int)((ulong)uVar2 >> 0x10);
  iVar4 = (int)uVar2;
  do {
    if (iVar6 == 0 && iVar4 == 0) {
      return iVar5;
    }
    if (*(int *)(iVar4 + 10) == param_2) {
      bVar3 = true;
      iVar5 = iVar4;
    }
    else {
      puVar1 = (undefined4 *)(iVar4 + 0x16);
      iVar6 = (int)((ulong)*puVar1 >> 0x10);
      iVar4 = (int)*puVar1;
    }
  } while (!bVar3);
  return iVar5;
}



int __cdecl16far FUN_16d7_048c(undefined4 param_1,int param_2)

{
  int *piVar1;
  int iVar2;
  bool bVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  undefined2 uVar7;
  int local_1a;
  
  bVar3 = false;
  local_1a = 0;
  uVar7 = (undefined2)((ulong)param_1 >> 0x10);
  iVar5 = *(int *)((int)param_1 + 0x3a);
  iVar6 = *(int *)((int)param_1 + 0x38);
  do {
    if (iVar5 == 0 && iVar6 == 0) {
      return local_1a;
    }
    iVar2 = *(int *)(iVar6 + 0x20);
    iVar4 = *(int *)(iVar6 + 0x1e);
    while ((!bVar3 && (iVar2 != 0 || iVar4 != 0))) {
      if (*(int *)(iVar4 + 4) == param_2) {
        bVar3 = true;
        local_1a = iVar4;
      }
      else {
        piVar1 = (int *)(iVar4 + 0xe);
        iVar2 = *(int *)(iVar4 + 0x10);
        iVar4 = *piVar1;
      }
    }
    piVar1 = (int *)(iVar6 + 0x16);
    iVar5 = *(int *)(iVar6 + 0x18);
    iVar6 = *piVar1;
  } while (!bVar3);
  return local_1a;
}



void __cdecl16far
FUN_16d7_0522(undefined2 param_1,undefined2 param_2,undefined2 param_3,int param_4)

{
  int iVar1;
  int in_DX;
  
  iVar1 = FUN_16d7_0446(param_1,param_2,param_3);
  if (in_DX != 0 || iVar1 != 0) {
    if (param_4 != 0) {
      *(byte *)(iVar1 + 0xc) = *(byte *)(iVar1 + 0xc) | 1;
      return;
    }
    *(byte *)(iVar1 + 0xc) = *(byte *)(iVar1 + 0xc) & 0xfe;
  }
  return;
}



void __cdecl16far
FUN_16d7_055a(undefined2 param_1,undefined2 param_2,undefined2 param_3,int param_4)

{
  byte *pbVar1;
  undefined2 in_DX;
  
  pbVar1 = (byte *)FUN_16d7_048c(param_1,param_2,param_3);
  if (param_4 != 0) {
    *pbVar1 = *pbVar1 | 1;
    return;
  }
  *pbVar1 = *pbVar1 & 0xfe;
  return;
}



void __cdecl16far FUN_16d7_058a(undefined4 param_1)

{
  byte *pbVar1;
  undefined4 *puVar2;
  byte *pbVar3;
  int iVar4;
  undefined2 uVar5;
  int iVar6;
  int iVar7;
  
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  iVar4 = *(int *)((int)param_1 + 0x38);
  iVar6 = *(int *)((int)param_1 + 0x3a);
  if (iVar6 != 0 || iVar4 != 0) {
    do {
      iVar7 = (int)((ulong)*(undefined4 *)(iVar4 + 0x1e) >> 0x10);
      pbVar3 = (byte *)(byte *)*(undefined4 *)(iVar4 + 0x1e);
      if (iVar7 != 0 || pbVar3 != (byte *)0x0) {
        do {
          *pbVar3 = *pbVar3 & 0xfe;
          pbVar1 = pbVar3 + 0xe;
          iVar7 = (int)((ulong)*(byte **)pbVar1 >> 0x10);
          pbVar3 = (byte *)*(byte **)pbVar1;
        } while (iVar7 != 0 || pbVar3 != (byte *)0x0);
      }
      puVar2 = (undefined4 *)(iVar4 + 0x16);
      iVar6 = (int)((ulong)*puVar2 >> 0x10);
      iVar4 = (int)*puVar2;
    } while (iVar6 != 0 || iVar4 != 0);
  }
  return;
}



void __cdecl16far
FUN_16d7_05ce(undefined2 param_1,undefined2 param_2,undefined2 param_3,int param_4)

{
  byte *pbVar1;
  undefined2 in_DX;
  
  pbVar1 = (byte *)FUN_16d7_048c(param_1,param_2,param_3);
  if (param_4 != 0) {
    *pbVar1 = *pbVar1 | 2;
    return;
  }
  *pbVar1 = *pbVar1 & 0xfd;
  return;
}



void __cdecl16far FUN_16d7_05fe(undefined4 param_1)

{
  byte *pbVar1;
  undefined4 *puVar2;
  byte *pbVar3;
  int iVar4;
  undefined2 uVar5;
  int iVar6;
  int iVar7;
  
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  iVar4 = *(int *)((int)param_1 + 0x38);
  iVar6 = *(int *)((int)param_1 + 0x3a);
  if (iVar6 != 0 || iVar4 != 0) {
    do {
      iVar7 = (int)((ulong)*(undefined4 *)(iVar4 + 0x1e) >> 0x10);
      pbVar3 = (byte *)(byte *)*(undefined4 *)(iVar4 + 0x1e);
      if (iVar7 != 0 || pbVar3 != (byte *)0x0) {
        do {
          *pbVar3 = *pbVar3 & 0xfd;
          pbVar1 = pbVar3 + 0xe;
          iVar7 = (int)((ulong)*(byte **)pbVar1 >> 0x10);
          pbVar3 = (byte *)*(byte **)pbVar1;
        } while (iVar7 != 0 || pbVar3 != (byte *)0x0);
      }
      puVar2 = (undefined4 *)(iVar4 + 0x16);
      iVar6 = (int)((ulong)*puVar2 >> 0x10);
      iVar4 = (int)*puVar2;
    } while (iVar6 != 0 || iVar4 != 0);
  }
  return;
}



undefined2 * __cdecl16far
FUN_16d7_0642(undefined4 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             int param_5)

{
  undefined2 *puVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  int iVar4;
  int iVar5;
  undefined2 uVar6;
  int iVar7;
  undefined2 *puVar8;
  undefined4 uVar9;
  int local_10;
  int iStack_e;
  int local_8;
  
  local_8 = 0;
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  iVar4 = (int)param_1;
  local_10 = *(int *)(iVar4 + 0x38);
  iStack_e = *(int *)(iVar4 + 0x3a);
  iVar5 = local_10;
  iVar7 = iStack_e;
  if (iStack_e != 0 || local_10 != 0) {
    do {
      iStack_e = iVar7;
      local_10 = iVar5;
      local_8 = *(int *)(local_10 + 2) + *(int *)(local_10 + 4);
      iVar7 = (int)((ulong)*(undefined4 *)(local_10 + 0x16) >> 0x10);
      iVar5 = (int)*(undefined4 *)(local_10 + 0x16);
    } while (iVar7 != 0 || iVar5 != 0);
  }
  iVar5 = *(int *)(iVar4 + 6);
  puVar8 = (undefined2 *)FUN_1f45_010a(iVar4 + 0x3c,uVar6);
  uVar3 = (undefined2)((ulong)puVar8 >> 0x10);
  puVar1 = (undefined2 *)puVar8;
  if (iStack_e == 0 && local_10 == 0) {
    *(undefined2 *)(iVar4 + 0x38) = puVar1;
    *(undefined2 *)(iVar4 + 0x3a) = uVar3;
  }
  else {
    *(undefined2 *)(local_10 + 0x16) = puVar1;
    *(undefined2 *)(local_10 + 0x18) = uVar3;
  }
  puVar1[0xd] = local_10;
  puVar1[0xe] = iStack_e;
  puVar1[0xc] = 0;
  puVar1[0xb] = 0;
  puVar1[0x10] = 0;
  puVar1[0xf] = 0;
  *puVar8 = 0;
  iVar7 = iVar4 + 0x3c;
  uVar2 = uVar6;
  FUN_2388_0dd4(param_2,param_3,iVar7,uVar6,param_2,param_3);
  uVar9 = FUN_1f45_010a(iVar7,uVar2);
  puVar1[7] = (int)uVar9;
  puVar1[8] = (int)((ulong)uVar9 >> 0x10);
  FUN_2388_0dec(uVar9);
  uVar2 = FUN_16d7_0246(param_2,param_3);
  puVar1[4] = uVar2;
  puVar1[3] = 10;
  puVar1[1] = local_8 + iVar5;
  iVar5 = FUN_16d7_00b6(iVar4 + 0x20,uVar6,param_2,param_3);
  puVar1[2] = iVar5 + *(int *)(iVar4 + 10) * 2;
  if (param_5 != 0) {
    puVar1[1] = (0x140 - puVar1[2]) - *(int *)(iVar4 + 6);
  }
  puVar1[9] = iVar4;
  puVar1[10] = uVar6;
  puVar1[5] = param_4;
  puVar1[6] = 0;
  *(int *)(iVar4 + 2) = *(int *)(iVar4 + 2) + 1;
  return puVar1;
}



byte * __cdecl16far
FUN_16d7_07de(undefined4 param_1,undefined2 param_2,char *param_3,undefined2 param_4)

{
  int *piVar1;
  int iVar2;
  undefined2 uVar3;
  int iVar4;
  int iVar5;
  byte *pbVar6;
  int iVar7;
  byte *pbVar8;
  undefined4 uVar9;
  undefined4 local_16;
  int local_10;
  
  iVar5 = 0;
  pbVar6 = (byte *)0x0;
  piVar1 = (int *)FUN_16d7_0446((int)param_1,param_1._2_2_,param_2);
  local_16 = (int *)CONCAT22(iVar5,piVar1);
  if (iVar5 != 0 || piVar1 != (int *)0x0) {
    local_10 = 0;
    iVar4 = piVar1[0xf];
    iVar2 = piVar1[0x10];
    if (piVar1[0x10] == 0 && piVar1[0xf] == 0) {
      iVar7 = 0;
    }
    else {
      do {
        local_10 = iVar2;
        iVar7 = iVar4;
        iVar2 = (int)((ulong)*(undefined4 *)(iVar7 + 0xe) >> 0x10);
        iVar4 = (int)*(undefined4 *)(iVar7 + 0xe);
      } while (iVar2 != 0 || iVar4 != 0);
    }
    pbVar8 = (byte *)FUN_1f45_010a((int)param_1 + 0x3c,param_1._2_2_);
    iVar4 = (int)((ulong)pbVar8 >> 0x10);
    pbVar6 = (byte *)pbVar8;
    if (local_10 == 0 && iVar7 == 0) {
      piVar1[0xf] = (int)pbVar6;
      piVar1[0x10] = iVar4;
    }
    else {
      *(undefined2 *)(iVar7 + 0xe) = pbVar6;
      *(int *)(iVar7 + 0x10) = iVar4;
    }
    *(int *)(pbVar6 + 0x12) = iVar7;
    *(int *)(pbVar6 + 0x14) = local_10;
    (pbVar6 + 0x10)[0] = 0;
    (pbVar6 + 0x10)[1] = 0;
    (pbVar6 + 0xe)[0] = 0;
    (pbVar6 + 0xe)[1] = 0;
    pbVar8[0] = 0;
    pbVar8[1] = 0;
    iVar2 = (int)param_1 + 0x3c;
    uVar3 = param_1._2_2_;
    FUN_2388_0dd4((char *)param_3,param_3._2_2_,iVar2,param_1._2_2_,(char *)param_3,param_3._2_2_);
    uVar9 = FUN_1f45_010a(iVar2,uVar3);
    *(int *)(pbVar6 + 6) = (int)uVar9;
    *(int *)(pbVar6 + 8) = (int)((ulong)uVar9 >> 0x10);
    FUN_2388_0dec(uVar9);
    if (*param_3 == '\0') {
      *pbVar8 = *pbVar8 | 1;
    }
    *(undefined2 *)(pbVar6 + 4) = param_4;
    uVar3 = FUN_16d7_0246((char *)param_3,param_3._2_2_);
    *(undefined2 *)(pbVar6 + 2) = uVar3;
    iVar4 = FUN_16d7_00b6((int)param_1 + 0x2c,param_1._2_2_,(char *)param_3,param_3._2_2_);
    iVar4 = *(int *)((int)param_1 + 0xc) * 2 + iVar4;
    if (iVar4 < piVar1[3]) {
      iVar4 = piVar1[3];
    }
    piVar1[3] = iVar4;
    *local_16 = *local_16 + 1;
  }
  return pbVar6;
}



void __cdecl16far FUN_16d7_0944(undefined4 param_1,int param_2,int param_3,int param_4)

{
  int iVar1;
  int iVar2;
  undefined1 uVar3;
  int iVar4;
  int iVar5;
  undefined2 uVar6;
  int iVar7;
  undefined2 unaff_DS;
  int local_e;
  int iStack_c;
  
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  iVar4 = (int)param_1;
  iVar2 = FUN_16d7_0008(*(undefined2 *)(iVar4 + 0x28),*(undefined2 *)(iVar4 + 0x2a));
  iVar2 = iVar2 + *(int *)(iVar4 + 4) + 1;
  uVar3 = (undefined1)((uint)iVar2 >> 8);
  FUN_16d7_0052(0,0,CONCAT11(uVar3,*(undefined1 *)(iVar4 + 0x10)),
                CONCAT11(uVar3,*(undefined1 *)(iVar4 + 0xe)),0x140,0,0,iVar2,*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  _local_e = CONCAT22(*(int *)(iVar4 + 0x3a),*(int *)(iVar4 + 0x38));
  if (*(int *)(iVar4 + 0x3a) != 0 || *(int *)(iVar4 + 0x38) != 0) {
    do {
      iVar7 = (int)((ulong)_local_e >> 0x10);
      iVar5 = (int)_local_e;
      if ((*(byte *)(iVar5 + 0xc) & 1) == 0) {
        iVar1 = *(int *)(iVar5 + 2);
        if ((iVar5 == param_2) && (iVar7 == param_3)) {
          uVar3 = (undefined1)((ulong)_local_e >> 0x18);
          FUN_16d7_0052(0,0,CONCAT11(uVar3,*(undefined1 *)(iVar4 + 0x1c)),
                        CONCAT11(uVar3,*(undefined1 *)(iVar4 + 0x1a)),0x140,0,0,iVar2,
                        *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                        *(undefined2 *)0x3afa);
        }
        FUN_16d7_010c(iVar4 + 0x20,uVar6,*(undefined2 *)(iVar5 + 0xe),*(undefined2 *)(iVar5 + 0x10),
                      *(int *)(iVar4 + 10) + iVar1,*(undefined2 *)(iVar4 + 4),0);
      }
      _local_e = CONCAT22(*(int *)(iVar5 + 0x18),*(int *)(iVar5 + 0x16));
    } while (*(int *)(iVar5 + 0x18) != 0 || *(int *)(iVar5 + 0x16) != 0);
  }
  if (param_4 != 0) {
    FUN_1c34_0044(iVar2,0x140,0);
  }
  return;
}



void __cdecl16far
FUN_16d7_0a6c(undefined4 param_1,int *param_2,int *param_3,int *param_4,int *param_5,int *param_6,
             int *param_7)

{
  byte *pbVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  byte *pbVar5;
  undefined2 uVar6;
  undefined2 unaff_DS;
  int local_8;
  
  *param_2 = *(int *)((int)param_1 + 2);
  uVar6 = (undefined2)((ulong)*(undefined4 *)((int)param_1 + 0x12) >> 0x10);
  iVar4 = (int)*(undefined4 *)((int)param_1 + 0x12);
  iVar2 = FUN_16d7_0008(*(undefined2 *)(iVar4 + 0x28),*(undefined2 *)(iVar4 + 0x2a));
  *param_3 = iVar2 + *(int *)(iVar4 + 4) + 3;
  local_8 = 0;
  pbVar5 = (byte *)*(undefined2 *)((int)param_1 + 0x1e);
  iVar2 = *(int *)((int)param_1 + 0x20);
  if (iVar2 != 0 || pbVar5 != (byte *)0x0) {
    local_8 = 0;
    do {
      if ((*pbVar5 & 2) == 0) {
        local_8 = local_8 + 1;
      }
      pbVar1 = pbVar5 + 0xe;
      iVar2 = (int)((ulong)*(byte **)pbVar1 >> 0x10);
      pbVar5 = (byte *)*(byte **)pbVar1;
    } while (iVar2 != 0 || pbVar5 != (byte *)0x0);
    unaff_DS = 0x25e7;
  }
  iVar2 = *(int *)((int)param_1 + 6) + 2;
  *param_4 = iVar2;
  iVar2 = iVar2 + *param_2 + -1;
  iVar3 = FUN_16d7_0008(*(undefined2 *)(iVar4 + 0x34),*(undefined2 *)(iVar4 + 0x36));
  iVar3 = (iVar3 + *(int *)(iVar4 + 8)) * local_8 + *(int *)(iVar4 + 8) + 2;
  *param_5 = iVar3;
  iVar3 = iVar3 + *param_3 + -1;
  if (0x13d < iVar2) {
    *param_2 = *param_2 + (0x13d - iVar2);
  }
  if (0xc5 < iVar3) {
    *param_3 = *param_3 + (199 - iVar3);
  }
  *param_6 = *param_2 + 1;
  *param_7 = *(int *)(iVar4 + 8) + *param_3 + 1;
  if ((*param_2 < 0) || (*param_3 < 0)) {
    FUN_1ed0_03d6(*param_3,*param_3 >> 0xf,*param_2,*param_2 >> 0xf);
  }
  return;
}



void __cdecl16far FUN_16d7_0b82(undefined4 param_1,byte *param_2,int param_3)

{
  int iVar1;
  undefined1 uVar2;
  int iVar3;
  int iVar4;
  byte *pbVar5;
  undefined2 uVar6;
  undefined2 unaff_DS;
  undefined2 uVar7;
  undefined2 uVar8;
  undefined2 uVar9;
  int local_26;
  int local_24;
  int local_22;
  int local_20;
  int local_1e;
  int local_1c;
  int local_1a;
  undefined2 uStack_18;
  undefined4 local_16;
  int local_12;
  int local_10;
  int local_e;
  int local_c;
  
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  iVar4 = (int)param_1;
  local_1a = *(int *)(iVar4 + 0x12);
  uStack_18 = *(undefined2 *)(iVar4 + 0x14);
  FUN_16d7_0a6c(iVar4,uVar6,&local_1e,&local_1c,&local_20,&local_24,&local_22,&local_26);
  iVar1 = local_24 + local_1c + -1;
  FUN_1c86_000c(CONCAT11((char)((uint)iVar1 >> 8),*(undefined1 *)(local_1a + 0x1e)),iVar1,
                *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                *(undefined2 *)0x3afa);
  iVar1 = local_1e + 1;
  iVar3 = local_1c + 1;
  local_10 = local_20 + -2;
  local_26 = iVar3;
  local_22 = iVar1;
  FUN_16d7_0052(0,0,*(undefined1 *)(local_1a + 0x14),*(undefined1 *)(local_1a + 0x12),local_20,
                local_1c,local_1e,local_24 + -2,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  pbVar5 = (byte *)*(undefined2 *)(iVar4 + 0x1e);
  local_16 = (byte *)CONCAT22(*(int *)(iVar4 + 0x20),pbVar5);
  local_12 = iVar1 + *(int *)(local_1a + 0xc);
  local_e = iVar3 + *(int *)(local_1a + 8);
  if (*(int *)(iVar4 + 0x20) != 0 || pbVar5 != (byte *)0x0) {
    do {
      if ((*local_16 & 2) == 0) {
        if ((param_2 == pbVar5) && (param_3 == (int)((ulong)local_16 >> 0x10))) {
          uVar6 = *(undefined2 *)0x3afa;
          uVar9 = *(undefined2 *)0x3af8;
          uVar8 = *(undefined2 *)0x3af6;
          uVar7 = *(undefined2 *)0x3af4;
          iVar1 = FUN_16d7_0008(*(undefined2 *)(local_1a + 0x34),*(undefined2 *)(local_1a + 0x36),
                                uVar7,uVar8,uVar9,uVar6);
          uVar2 = (undefined1)((uint)(iVar1 + 2) >> 8);
          FUN_16d7_0052(0,0,CONCAT11(uVar2,*(undefined1 *)(local_1a + 0x18)),
                        CONCAT11(uVar2,*(undefined1 *)(local_1a + 0x16)),local_20,local_1c,local_1e,
                        iVar1 + 2,uVar7,uVar8,uVar9,uVar6);
        }
        uVar6 = (undefined2)((ulong)local_16 >> 0x10);
        pbVar5 = (byte *)local_16;
        if (**(char **)(pbVar5 + 6) == '\0') {
          iVar1 = FUN_16d7_0008(*(undefined2 *)(local_1a + 0x34),*(undefined2 *)(local_1a + 0x36));
          local_c = (iVar1 >> 1) + local_e;
          FUN_1c5b_0004(CONCAT11((char)((uint)local_c >> 8),*(undefined1 *)(local_1a + 0x2e)),1,
                        *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                        *(undefined2 *)0x3afa);
        }
        else {
          FUN_16d7_010c(local_1a + 0x2c,uStack_18,*(undefined2 *)(pbVar5 + 6),
                        *(undefined2 *)(pbVar5 + 8),local_12,local_e,*local_16 & 1);
        }
        iVar1 = FUN_16d7_0008(*(undefined2 *)(local_1a + 0x34),*(undefined2 *)(local_1a + 0x36));
        local_e = local_e + iVar1 + *(int *)(local_1a + 8);
      }
      uVar6 = (undefined2)((ulong)local_16 >> 0x10);
      pbVar5 = *(byte **)((byte *)local_16 + 0xe);
      iVar1 = *(int *)((byte *)local_16 + 0x10);
      local_16 = (byte *)CONCAT22(iVar1,pbVar5);
    } while (iVar1 != 0 || pbVar5 != (byte *)0x0);
  }
  FUN_1c34_0044(local_24,local_20,local_1c);
  return;
}



void __stdcall16far FUN_16d7_0d9c(undefined4 param_1)

{
  uint *puVar1;
  int iVar2;
  bool bVar3;
  bool bVar4;
  uint uVar5;
  uint uVar6;
  int iVar7;
  uint *puVar8;
  undefined2 uVar9;
  undefined2 uVar10;
  undefined2 unaff_DS;
  uint local_3e;
  undefined4 local_36;
  int local_2e;
  undefined4 local_2c;
  int local_28;
  int local_26;
  int local_24;
  int local_22;
  int local_20;
  int local_1e;
  uint local_1c;
  int local_1a;
  undefined2 local_18;
  int local_16;
  int local_14;
  undefined1 local_12 [2];
  undefined4 local_10;
  uint local_c;
  int local_a;
  undefined4 local_8;
  int local_4;
  
  local_22 = 0;
  local_2c = (uint *)0x0;
  uVar9 = (undefined2)((ulong)param_1 >> 0x10);
  local_10 = (uint *)CONCAT22(*(undefined2 *)((int)param_1 + 0x14),
                              (uint *)*(undefined2 *)((int)param_1 + 0x12));
  local_c = 1;
  local_1c = DAT_0000_0417 & 8;
  do {
    local_16 = 0;
    bVar4 = true;
    uVar9 = (undefined2)((ulong)param_1 >> 0x10);
    iVar7 = (int)param_1;
    if (*(int *)(iVar7 + 0x20) == 0 && *(int *)(iVar7 + 0x1e) == 0) goto LAB_16d7_1396;
    FUN_16d7_0944((uint *)local_10,local_10._2_2_,iVar7,uVar9,1);
    FUN_16d7_0a6c(iVar7,uVar9,&local_1e,&local_20,&local_28,&local_2e,local_12,&local_1a);
    uVar10 = (undefined2)((ulong)local_10 >> 0x10);
    local_a = FUN_16d7_0008(((uint *)local_10)[0x1a],((uint *)local_10)[0x1b]);
    local_a = local_a + ((uint *)local_10)[4];
    local_18 = FUN_1c9d_000e(local_2e,local_28,local_20,local_1e);
    local_26 = *(int *)0x72a;
    if (local_26 == 0) {
      local_2c = (uint *)CONCAT22(*(undefined2 *)(iVar7 + 0x20),
                                  (uint *)*(undefined2 *)(iVar7 + 0x1e));
    }
    do {
      FUN_1000_0004();
      FUN_1c21_0042();
      if (*(int *)0x732 != 0) {
        bVar3 = false;
        uVar9 = (undefined2)((ulong)local_10 >> 0x10);
        iVar7 = FUN_16d7_0008(((uint *)local_10)[0x14],((uint *)local_10)[0x15]);
        uVar9 = (undefined2)((ulong)local_10 >> 0x10);
        puVar8 = (uint *)local_10;
        if (*(int *)0x726 <= (int)(iVar7 + puVar8[2] + 1)) {
          uVar5 = puVar8[0x1c];
          uVar6 = puVar8[0x1d];
          do {
            local_8 = CONCAT22(uVar6,uVar5);
            while( true ) {
              if ((local_8._2_2_ == 0 && (int)local_8 == 0) || (bVar3)) goto LAB_16d7_0f12;
              local_4 = *(int *)((int)local_8 + 2);
              local_24 = local_4 + *(int *)((int)local_8 + 4);
              if ((local_24 < *(int *)0x724) || ((*(byte *)((int)local_8 + 0xc) & 1) != 0)) break;
              if (((int)param_1 == (int)local_8) && (param_1._2_2_ == local_8._2_2_)) {
                local_8 = 0;
              }
              else {
                bVar3 = true;
              }
            }
            uVar5 = *(uint *)((int)local_8 + 0x16);
            uVar6 = *(uint *)((int)local_8 + 0x18);
          } while( true );
        }
LAB_16d7_0f12:
        if (bVar3) {
          param_1 = local_8;
          local_16 = 1;
          local_22 = 0;
        }
        else if ((((*(int *)0x726 < local_20) || (local_20 + local_2e + -1 < *(int *)0x726)) ||
                 (*(int *)0x724 < local_1e)) || (local_1e + local_28 + -1 < *(int *)0x724)) {
          bVar4 = true;
          local_2c = (uint *)0x0;
          local_22 = 0;
        }
        else {
          local_36 = (uint *)CONCAT22(*(undefined2 *)((int)param_1 + 0x20),
                                      (uint *)*(undefined2 *)((int)param_1 + 0x1e));
          bVar3 = false;
          local_14 = local_1a;
          while (!bVar3) {
            if (local_36._2_2_ == 0 && (uint *)local_36 == (uint *)0x0) break;
            if ((*local_36 & 2) == 0) {
              if (((local_14 + -1 <= *(int *)0x726) && (*(int *)0x726 < local_a + local_14 + -1)) &&
                 (((*local_36 & 1) == 0 && (**(char **)((uint *)local_36 + 3) != '\0')))) {
                local_2c = local_36;
                bVar3 = true;
                bVar4 = true;
                local_22 = 0;
              }
              local_14 = local_14 + local_a;
            }
            local_36 = (uint *)CONCAT22(((uint *)local_36)[8],(uint *)((uint *)local_36)[7]);
          }
        }
      }
      iVar7 = FUN_1baf_0004();
      if (((iVar7 == 0) || (local_c == 0)) || (local_16 != 0)) goto LAB_16d7_10e2;
      local_3e = FUN_1baf_0018();
      if (((int)local_3e < 0x100) && ((*(byte *)(local_3e + 0x45a9) & 2) != 0)) {
        local_3e = local_3e - 0x20;
      }
      local_22 = 0;
      iVar7 = (int)param_1;
      uVar9 = (undefined2)((ulong)param_1 >> 0x10);
      if (local_3e == 0x38) {
LAB_16d7_1062:
        bVar3 = false;
        bVar4 = false;
        do {
          if (local_2c._2_2_ != 0 || (uint *)local_2c != (uint *)0x0) {
            local_2c = (uint *)CONCAT22(((uint *)local_2c)[10],(uint *)((uint *)local_2c)[9]);
          }
          if (local_2c._2_2_ == 0 && (uint *)local_2c == (uint *)0x0) {
            uVar5 = *(uint *)(iVar7 + 0x20);
            puVar8 = (uint *)*(undefined2 *)(iVar7 + 0x1e);
            while (local_2c = (uint *)CONCAT22(uVar5,puVar8), puVar8[8] != 0 || puVar8[7] != 0) {
              puVar1 = puVar8 + 7;
              uVar5 = puVar8[8];
              puVar8 = (uint *)*puVar1;
            }
            if (bVar4) {
              bVar3 = true;
            }
            bVar4 = true;
          }
        } while ((((*local_2c & 3) != 0) || (**(char **)((uint *)local_2c + 3) == '\0')) && (!bVar3)
                );
LAB_16d7_10dd:
        bVar4 = true;
      }
      else if ((int)local_3e < 0x39) {
        if (local_3e == 0x32) {
LAB_16d7_112c:
          bVar3 = false;
          bVar4 = false;
          do {
            if (local_2c._2_2_ != 0 || (uint *)local_2c != (uint *)0x0) {
              local_2c = (uint *)CONCAT22(((uint *)local_2c)[8],(uint *)((uint *)local_2c)[7]);
            }
            if (local_2c._2_2_ == 0 && (uint *)local_2c == (uint *)0x0) {
              local_2c = (uint *)CONCAT22(*(undefined2 *)(iVar7 + 0x20),
                                          (uint *)*(undefined2 *)(iVar7 + 0x1e));
              if (bVar4) {
                bVar3 = true;
              }
              bVar4 = true;
            }
          } while ((((*local_2c & 3) != 0) || (**(char **)((uint *)local_2c + 3) == '\0')) &&
                  (!bVar3));
          goto LAB_16d7_10dd;
        }
        if (0x32 < local_3e) {
LAB_16d7_12a3:
          bVar3 = false;
          uVar5 = *(uint *)(iVar7 + 0x20);
          puVar8 = (uint *)*(undefined2 *)(iVar7 + 0x1e);
          do {
            local_36 = (uint *)CONCAT22(uVar5,puVar8);
            while( true ) {
              if ((bVar3) || (uVar5 == 0 && puVar8 == (uint *)0x0)) {
                if (!bVar3) goto LAB_16d7_10e2;
                local_2c = (uint *)CONCAT22(uVar5,puVar8);
                local_c = 0;
                goto LAB_16d7_10dd;
              }
              if ((puVar8[1] != local_3e) || ((*local_36 & 3) != 0)) break;
              bVar3 = true;
            }
            puVar1 = puVar8 + 7;
            uVar5 = puVar8[8];
            puVar8 = (uint *)*puVar1;
          } while( true );
        }
        if ((char)local_3e == '\r') {
          local_c = 0;
        }
        else {
          if ((char)local_3e != '\x1b') goto LAB_16d7_12a3;
          local_c = 0;
          local_2c = (uint *)0x0;
        }
      }
      else {
        if (local_3e == 0x148) goto LAB_16d7_1062;
        puVar8 = (uint *)local_10;
        uVar10 = (undefined2)((ulong)local_10 >> 0x10);
        if (local_3e == 0x14b) {
          do {
            uVar9 = (undefined2)((ulong)param_1 >> 0x10);
            iVar7 = *(int *)((int)param_1 + 0x1a);
            iVar2 = *(int *)((int)param_1 + 0x1c);
            param_1 = CONCAT22(iVar2,iVar7);
            if (iVar2 == 0 && iVar7 == 0) {
              uVar5 = puVar8[0x1d];
              uVar6 = puVar8[0x1c];
              while (param_1 = CONCAT22(uVar5,uVar6),
                    *(int *)(uVar6 + 0x18) != 0 || *(int *)(uVar6 + 0x16) != 0) {
                puVar1 = (uint *)(uVar6 + 0x16);
                uVar5 = *(uint *)(uVar6 + 0x18);
                uVar6 = *puVar1;
              }
            }
          } while ((*(byte *)((int)param_1 + 0xc) & 1) != 0);
        }
        else {
          if (local_3e != 0x14d) {
            if (local_3e != 0x150) goto LAB_16d7_12a3;
            goto LAB_16d7_112c;
          }
          do {
            uVar9 = (undefined2)((ulong)param_1 >> 0x10);
            iVar7 = *(int *)((int)param_1 + 0x16);
            iVar2 = *(int *)((int)param_1 + 0x18);
            param_1 = CONCAT22(iVar2,iVar7);
            if (iVar2 == 0 && iVar7 == 0) {
              param_1 = CONCAT22(puVar8[0x1d],puVar8[0x1c]);
            }
          } while ((*(byte *)((int)param_1 + 0xc) & 1) != 0);
        }
        local_16 = 1;
      }
LAB_16d7_10e2:
      if ((local_22 != 0) && ((DAT_0000_0417 & 8) == 0)) {
        if (local_1c == (DAT_0000_0417 & 8)) {
          local_c = DAT_0000_0417 & 8;
        }
        local_1c = 0;
      }
      if ((((DAT_0000_0417 & 8) == 0) || (bVar4)) || (local_16 != 0)) {
        local_22 = 0;
      }
      else {
        local_22 = 1;
      }
      if ((bVar4) && (local_16 == 0)) {
        FUN_16d7_0b82((int)param_1,param_1._2_2_,(uint *)local_2c,local_2c._2_2_);
        bVar4 = false;
      }
      if (*(int *)0x730 != 0) {
        if (((local_2c._2_2_ != 0 || (uint *)local_2c != (uint *)0x0) || (local_26 == 0)) ||
           (uVar9 = (undefined2)((ulong)local_10 >> 0x10),
           iVar7 = FUN_16d7_0008(((uint *)local_10)[0x14],((uint *)local_10)[0x15]),
           (int)(iVar7 + ((uint *)local_10)[2] + 1) < *(int *)0x726)) {
          local_c = 0;
        }
        local_26 = 0;
      }
      FUN_1c21_0110();
    } while ((local_c != 0) && (local_16 == 0));
    FUN_1c9d_00ec(local_2e,local_28,local_20,local_1e);
    FUN_1c34_0044(local_2e,local_28,local_20);
  } while (local_c != 0);
  if (local_2c._2_2_ == 0 && (uint *)local_2c == (uint *)0x0) {
    *local_10 = 0;
  }
  else {
    *local_10 = ((uint *)local_2c)[2];
  }
LAB_16d7_1396:
  FUN_1c21_002a();
  FUN_16d7_0944((uint *)local_10,local_10._2_2_,0,0,1);
  return;
}



undefined2 __stdcall16far FUN_16d7_13b4(undefined2 *param_1,undefined2 param_2)

{
  undefined4 *puVar1;
  bool bVar2;
  int in_AX;
  int iVar3;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int iVar4;
  undefined2 local_8;
  
  local_8 = 0;
  *param_1 = 0;
  if ((*(int *)0x728 != 0) &&
     (iVar3 = FUN_16d7_0008(param_1[0x14],param_1[0x15]), *(int *)0x726 <= param_1[2] + iVar3 + 1))
  {
    bVar2 = false;
    iVar4 = (int)((ulong)*(undefined4 *)(param_1 + 0x1c) >> 0x10);
    iVar3 = (int)*(undefined4 *)(param_1 + 0x1c);
    if (iVar4 != 0 || iVar3 != 0) {
      do {
        if (bVar2) break;
        if ((*(int *)(iVar3 + 2) + *(int *)(iVar3 + 4) < *(int *)0x724) ||
           ((*(byte *)(iVar3 + 0xc) & 1) != 0)) {
          puVar1 = (undefined4 *)(iVar3 + 0x16);
          iVar4 = (int)((ulong)*puVar1 >> 0x10);
          iVar3 = (int)*puVar1;
        }
        else {
          bVar2 = true;
        }
      } while (iVar4 != 0 || iVar3 != 0);
    }
    if ((bVar2) && (local_8 = 1, in_AX != 0)) {
      FUN_16d7_0d9c(iVar3,iVar4);
    }
  }
  return local_8;
}



bool __stdcall16far FUN_16d7_1452(undefined2 *param_1,undefined2 param_2)

{
  bool bVar1;
  int in_AX;
  int iVar2;
  int iVar3;
  int iVar4;
  int local_a;
  
  iVar4 = param_1[0x1c];
  local_a = param_1[0x1d];
  *param_1 = 0;
  iVar2 = FUN_1bb2_000e();
  if ((iVar2 == in_AX) || (local_a == 0 && iVar4 == 0)) {
    bVar1 = false;
  }
  else {
    bVar1 = false;
    iVar3 = iVar4;
    do {
      iVar4 = iVar3;
      if (bVar1) break;
      if ((*(int *)(iVar3 + 8) == iVar2) && ((*(byte *)(iVar3 + 0xc) & 1) == 0)) {
        bVar1 = true;
      }
      else {
        iVar4 = *(int *)(iVar3 + 0x16);
        local_a = *(int *)(iVar3 + 0x18);
      }
      iVar3 = iVar4;
    } while (local_a != 0 || iVar4 != 0);
  }
  if (bVar1) {
    FUN_16d7_0d9c(iVar4,local_a);
  }
  FUN_1c21_002a();
  return bVar1;
}



undefined2 __stdcall16far FUN_16d7_14e6(undefined2 *param_1)

{
  int *piVar1;
  bool bVar2;
  int in_AX;
  int iVar3;
  byte *pbVar4;
  undefined2 uVar5;
  int local_12;
  byte *local_10;
  int local_e;
  
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  local_e = 0;
  iVar3 = ((undefined2 *)param_1)[0x1c];
  local_12 = ((undefined2 *)param_1)[0x1d];
  *param_1 = 0;
  bVar2 = false;
  local_10 = (byte *)0x0;
  if (local_12 != 0 || iVar3 != 0) {
    local_10 = (byte *)0x0;
    do {
      if (bVar2) break;
      if ((*(byte *)(iVar3 + 0xc) & 1) == 0) {
        pbVar4 = (byte *)*(undefined2 *)(iVar3 + 0x1e);
        local_e = *(int *)(iVar3 + 0x20);
        local_10 = pbVar4;
        if (local_e != 0 || pbVar4 != (byte *)0x0) {
          do {
            local_10 = pbVar4;
            if (bVar2) break;
            if ((*(int *)(pbVar4 + 2) == in_AX) && ((*pbVar4 & 3) == 0)) {
              bVar2 = true;
            }
            else {
              local_10 = *(byte **)(pbVar4 + 0xe);
              local_e = *(int *)(pbVar4 + 0x10);
            }
            pbVar4 = local_10;
          } while (local_e != 0 || local_10 != (byte *)0x0);
        }
      }
      piVar1 = (int *)(iVar3 + 0x16);
      local_12 = *(int *)(iVar3 + 0x18);
      iVar3 = *piVar1;
    } while (local_12 != 0 || *piVar1 != 0);
  }
  uVar5 = 0;
  if (bVar2) {
    uVar5 = *(undefined2 *)(local_10 + 4);
    *param_1 = uVar5;
  }
  return uVar5;
}



void __cdecl16far FUN_16d7_15ac(void)

{
  int iVar1;
  int in_DX;
  undefined2 unaff_DS;
  undefined1 local_3;
  
  iVar1 = FUN_1ddb_0008();
  if (in_DX != 0 || iVar1 != 0) {
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x614 = (uint)local_3;
    *(uint *)0x618 = (uint)local_3;
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x616 = (uint)local_3;
    *(uint *)0x61a = (uint)local_3;
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x620 = (uint)local_3;
    *(uint *)0x61c = (uint)local_3;
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x622 = (uint)local_3;
    *(uint *)0x61e = (uint)local_3;
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x62c = (uint)local_3;
    *(uint *)0x626 = (uint)local_3;
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x630 = (uint)local_3;
    *(uint *)0x62a = (uint)local_3;
    FUN_1d8f_0000(0,iVar1,in_DX);
    *(uint *)0x62c = (uint)local_3;
    *(uint *)0x628 = (uint)local_3;
    FUN_1cc9_0310(iVar1,in_DX);
  }
  return;
}



void __cdecl16far FUN_1842_0000(void)

{
  undefined2 unaff_DS;
  
  if (*(int *)0x640 != 0) {
    FUN_2388_02c2(*(undefined2 *)0x640);
    *(undefined2 *)0x640 = 0;
  }
  return;
}



int __cdecl16far FUN_1842_001a(int param_1,int param_2)

{
  bool bVar1;
  bool bVar2;
  int iVar3;
  int iVar4;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 local_a4 [80];
  undefined1 local_54;
  undefined1 local_53;
  int local_4;
  
  local_4 = 1;
  bVar1 = false;
  local_54 = 0x40;
  local_53 = 0;
  FUN_2388_05e6(&local_54,param_2);
  FUN_2388_0a88(&local_54);
  if (param_1 == 0) {
    bVar1 = true;
  }
  else {
    FUN_1842_0000();
    FUN_2388_0626(local_a4,param_1);
    FUN_1c04_000e(0x642,unaff_DS,local_a4,unaff_SS);
    iVar3 = FUN_1297_0104(local_a4,unaff_SS);
    *(int *)0x640 = iVar3;
    iVar4 = local_4;
    if (iVar3 == 0) goto LAB_1842_00f8;
  }
  if (param_2 != 0) {
    bVar2 = false;
    do {
      iVar4 = FUN_2388_078a(0x5ec6,0x50,*(undefined2 *)0x640);
      if (iVar4 == 0) {
        iVar4 = local_4;
        if (!bVar1) goto LAB_1842_00f8;
        bVar1 = false;
        *(undefined1 *)0x5ec6 = 0;
      }
      FUN_1bc4_0000(0x5ec6,unaff_DS);
      FUN_1c1a_0002(0x5ec6,unaff_DS);
      iVar4 = FUN_2388_0658(0x5ec6,&local_54);
      if (iVar4 == 0) {
        bVar2 = true;
      }
    } while (!bVar2);
    iVar4 = FUN_2388_0684(0x5ec6);
    *(int *)0x5ad6 = iVar4 + 0x5ec6;
  }
  iVar4 = 0;
LAB_1842_00f8:
  if (iVar4 != 0) {
    FUN_1842_0000();
  }
  return iVar4;
}



int __cdecl16far FUN_1842_0106(void)

{
  int iVar1;
  undefined1 *puVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  iVar3 = 0;
  iVar1 = FUN_2388_078a(0x5ec6,0x50,*(undefined2 *)0x640);
  if (iVar1 != 0) {
    FUN_1bc4_0000(0x5ec6,unaff_DS);
    FUN_1c1a_0002(0x5ec6,unaff_DS);
    while (puVar2 = (undefined1 *)FUN_2388_09e8(0x5ec6,0x5f), puVar2 != (undefined1 *)0x0) {
      *puVar2 = 0x20;
    }
    iVar3 = 0x5ec6;
    *(undefined2 *)0x5ad6 = 0x5ec6;
  }
  if (iVar3 == 0) {
    FUN_1842_0000();
  }
  return iVar3;
}



undefined2 __cdecl16far FUN_1842_015e(void)

{
  char cVar1;
  char *pcVar2;
  char *pcVar3;
  undefined2 unaff_DS;
  
  pcVar2 = (char *)*(undefined2 *)0x5ad6;
  pcVar3 = (char *)0x51cc;
  cVar1 = *pcVar2;
  while ((cVar1 != '\0' && (*pcVar2 != ','))) {
    *pcVar3 = *pcVar2;
    pcVar3 = pcVar3 + 1;
    pcVar2 = pcVar2 + 1;
    cVar1 = *pcVar2;
  }
  if (*pcVar2 != '\0') {
    pcVar2 = pcVar2 + 1;
  }
  *(undefined2 *)0x5ad6 = pcVar2;
  *pcVar3 = '\0';
  FUN_1c1a_0002(0x51cc,unaff_DS);
  return 0x51cc;
}



void __cdecl16far FUN_1842_0198(void)

{
  FUN_1842_015e();
  FUN_1d12_000c();
  return;
}



void __cdecl16far FUN_1842_01c8(void)

{
  FUN_1842_015e();
  FUN_1334_0036(0x51cc);
  return;
}



char __cdecl16far FUN_1842_01da(void)

{
  char *pcVar1;
  undefined2 unaff_DS;
  char local_3;
  
  local_3 = '\0';
  FUN_1842_015e();
  for (pcVar1 = (char *)0x51cc; (*pcVar1 == '0' || (*pcVar1 == '1')); pcVar1 = pcVar1 + 1) {
    local_3 = local_3 * '\x02';
    if (*pcVar1 == '1') {
      local_3 = local_3 + '\x01';
    }
  }
  return local_3;
}



undefined2 __cdecl16far FUN_1842_0208(undefined2 param_1,undefined2 param_2,int param_3)

{
  int iVar1;
  undefined2 uVar2;
  
  uVar2 = 1;
  iVar1 = FUN_1842_001a(param_1,param_2);
  if (iVar1 == 0) {
    if (-1 < param_3) {
      param_3 = param_3 + 1;
      do {
        FUN_1842_0106();
        param_3 = param_3 + -1;
      } while (param_3 != 0);
    }
    uVar2 = 0;
  }
  FUN_1842_0000();
  return uVar2;
}



int __cdecl16far FUN_1865_000e(int param_1,int param_2,int param_3)

{
  if (param_2 < param_1) {
    param_2 = param_1;
  }
  if (param_3 < param_2) {
    param_2 = param_3;
  }
  return param_2;
}



void __cdecl16far FUN_1865_002c(undefined2 *param_1,undefined2 *param_2)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  
  uVar1 = *param_1;
  *param_1 = *param_2;
  *param_2 = uVar1;
  return;
}



int __cdecl16far FUN_1865_0042(uint param_1,uint param_2)

{
  if ((int)param_1 < 1) {
    param_1 = ~param_1 + 1;
  }
  if ((int)param_2 < 1) {
    param_2 = ~param_2 + 1;
  }
  if ((int)param_2 < (int)param_1) {
    return ((int)param_2 >> 1) + param_1;
  }
  return ((int)param_1 >> 1) + param_2;
}



void __cdecl16far FUN_1865_007e(int param_1,int param_2,int param_3,int param_4)

{
  uint uVar1;
  uint uVar2;
  
  uVar1 = param_1 - param_3;
  if ((int)uVar1 < 1) {
    uVar1 = ~uVar1 + 1;
  }
  uVar2 = param_2 - param_4;
  if ((int)uVar2 < 1) {
    uVar2 = ~uVar2 + 1;
  }
  FUN_1865_0042(uVar1,uVar2);
  return;
}



uint __cdecl16far FUN_1865_00c6(uint param_1,uint param_2)

{
  if ((int)param_1 < 1) {
    param_1 = ~param_1 + 1;
  }
  if ((int)param_2 < 1) {
    param_2 = ~param_2 + 1;
  }
  if ((int)param_2 < (int)param_1) {
    return param_1;
  }
  return param_2;
}



void __cdecl16far FUN_1865_00f6(int param_1,int param_2,int param_3,int param_4)

{
  uint uVar1;
  uint uVar2;
  
  uVar1 = param_1 - param_3;
  if ((int)uVar1 < 1) {
    uVar1 = ~uVar1 + 1;
  }
  uVar2 = param_2 - param_4;
  if ((int)uVar2 < 1) {
    uVar2 = ~uVar2 + 1;
  }
  FUN_1865_00c6(uVar1,uVar2);
  return;
}



undefined2 __cdecl16far FUN_1865_013e(uint param_1,char param_2)

{
  undefined2 uVar1;
  
  uVar1 = 0;
  if ((((byte)(param_2 + 1) & 7) == param_1) || (((byte)(param_2 - 1) & 7) == param_1)) {
    uVar1 = 1;
  }
  return uVar1;
}



void __cdecl16far FUN_187b_004a(int *param_1,int *param_2,int *param_3,int *param_4)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  iVar1 = *param_3 + *param_1 + -1;
  iVar2 = *param_4 + *param_2 + -1;
  iVar3 = *(int *)0x6026;
  if (iVar1 < *(int *)0x6026) {
    iVar3 = iVar1;
  }
  iVar1 = *param_1;
  if (iVar1 < *(int *)0x49f2) {
    iVar1 = *(int *)0x49f2;
  }
  *param_1 = iVar1;
  iVar1 = (iVar3 - iVar1) + 1;
  *param_3 = iVar1;
  iVar3 = *(int *)0x603e;
  if (iVar2 < *(int *)0x603e) {
    iVar3 = iVar2;
  }
  iVar2 = *param_2;
  if (iVar2 < *(int *)0x49f4) {
    iVar2 = *(int *)0x49f4;
  }
  *param_2 = iVar2;
  iVar3 = (iVar3 - iVar2) + 1;
  *param_4 = iVar3;
  if (iVar1 < 0) {
    iVar1 = 0;
  }
  *param_3 = iVar1;
  if (iVar3 < 0) {
    iVar3 = 0;
  }
  *param_4 = iVar3;
  return;
}



void __cdecl16far FUN_187b_00c0(undefined2 param_1,int param_2,int param_3,int param_4)

{
  undefined2 unaff_DS;
  
  FUN_1c67_0000(param_4 * *(int *)0x4e8c,param_3 * *(int *)0x4e8a,
                ((*(int *)0x5af4 - *(int *)0x49f4) + param_2) * *(int *)0x4e8c + 8,
                *(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                *(undefined2 *)0x3afa,*(undefined2 *)0x3afc,*(undefined2 *)0x3afe,
                *(undefined2 *)0x3b00,*(undefined2 *)0x3b02);
  return;
}



void __cdecl16far FUN_187b_012a(void)

{
  undefined2 unaff_DS;
  
  FUN_1c67_0000(*(undefined2 *)0x64ee,*(undefined2 *)0x64ec,8,*(undefined2 *)0x3af4,
                *(undefined2 *)0x3af6,*(undefined2 *)0x3af8,*(undefined2 *)0x3afa,
                *(undefined2 *)0x3afc,*(undefined2 *)0x3afe,*(undefined2 *)0x3b00,
                *(undefined2 *)0x3b02);
  return;
}



void __cdecl16far FUN_187b_0160(void)

{
  int iVar1;
  int iVar2;
  uint uVar3;
  int extraout_DX;
  int iVar4;
  undefined2 unaff_DS;
  int local_12;
  uint local_e;
  uint local_8;
  int local_6;
  
  local_e = 1;
  local_12 = 0;
  local_6 = 0;
  local_8 = 0;
  iVar1 = *(int *)0x52d8;
  iVar2 = *(int *)0x52d4;
  *(undefined2 *)0x50b8 = 0;
  *(undefined2 *)0x50b6 = 0;
  do {
    uVar3 = local_e & 1;
    local_e = local_e >> 1;
    if (uVar3 != 0) {
      local_e = local_e ^ 0xb400;
    }
    if ((local_e - 1 < (uint)(iVar1 * iVar2)) &&
       (FUN_1c34_0044(*(undefined2 *)0x4e8c,*(undefined2 *)0x4e8a,
                      ((local_e - 1) / *(uint *)0x52d4 + *(int *)0x5af4) * *(int *)0x4e8c + 8),
       (uint)(iVar1 * iVar2) < 0xb5)) {
      iVar4 = extraout_DX;
      if (local_6 != 0 || local_8 != 0) {
        do {
          do {
            uVar3 = FUN_1d18_0022();
            iVar4 = (iVar4 - local_6) - (uint)(uVar3 < local_8);
          } while (iVar4 < 0);
        } while ((iVar4 < 1) && (uVar3 == local_8));
      }
      local_8 = FUN_1d18_0022();
      local_6 = iVar4;
    }
    local_12 = local_12 + 1;
  } while (local_12 != 0);
  return;
}



void __cdecl16far FUN_187b_021c(void)

{
  undefined2 unaff_DS;
  
  FUN_1c34_0044(*(undefined2 *)0x64ee,*(undefined2 *)0x64ec,8);
  return;
}



void __cdecl16far FUN_187b_0234(undefined2 param_1,int param_2,int param_3,int param_4)

{
  undefined2 unaff_DS;
  
  FUN_1c34_0044(param_4 * *(int *)0x4e8c,param_3 * *(int *)0x4e8a,
                ((*(int *)0x5af4 - *(int *)0x49f4) + param_2) * *(int *)0x4e8c + 8);
  return;
}



void __cdecl16far
FUN_18a2_000a(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  FUN_187b_004a(&param_1,&param_2,&param_3,&param_4);
  FUN_1a47_0d66(0xffff,param_4);
  FUN_187b_00c0(param_1,param_2,param_3,param_4);
  FUN_187b_0234(param_1,param_2,param_3,param_4);
  FUN_1b56_03b4(1,0xffff);
  return;
}



void __cdecl16far FUN_18a2_0068(void)

{
  undefined2 unaff_DS;
  
  FUN_1c86_000c(0,*(int *)0x64ee + 8,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  FUN_1a47_1022();
  FUN_187b_012a();
  FUN_187b_021c();
  FUN_1b56_03b4(1,0xffff);
  return;
}



void __cdecl16far FUN_18a2_00b0(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x64a);
  return;
}



void __cdecl16far FUN_18ad_0032(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x64c);
  return;
}



void __cdecl16far FUN_18ad_0042(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x64f);
  return;
}



void __cdecl16far FUN_18ad_0052(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x652);
  return;
}



void __cdecl16far FUN_18ad_0062(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x656);
  return;
}



void __cdecl16far FUN_18ad_0072(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x658);
  return;
}



void __cdecl16far FUN_18ad_0082(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x65a);
  return;
}



void __cdecl16far FUN_18ad_0092(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x65c);
  return;
}



void __cdecl16far FUN_18ad_00a2(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x65e);
  return;
}



void __cdecl16far FUN_18ad_00b2(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x660);
  return;
}



void __cdecl16far FUN_18ad_00c2(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x662);
  return;
}



void __cdecl16far FUN_18ad_00d2(undefined2 param_1)

{
  FUN_2388_05e6(param_1,0x664);
  return;
}



void __cdecl16far FUN_18ad_00e2(undefined2 param_1,undefined2 param_2)

{
  FUN_1334_006c(param_2);
  FUN_2388_0e22(param_1);
  return;
}



void __cdecl16far FUN_18ad_00fc(undefined2 param_1,undefined2 param_2)

{
  FUN_18ad_0092(param_1);
  FUN_1334_006c(param_2);
  FUN_2388_0e22(param_1);
  FUN_18ad_00a2(param_1);
  return;
}



void __cdecl16far FUN_18ad_012e(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  undefined1 local_16 [20];
  
  FUN_2388_073c(param_3,local_16,10);
  FUN_2388_0e22(param_1,param_2,local_16);
  return;
}



void __cdecl16far FUN_18ad_0156(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  int iVar1;
  int iVar2;
  undefined1 local_1a [20];
  undefined2 local_6;
  int local_4;
  
  FUN_2388_073c(param_3,local_1a,2);
  local_6 = 0;
  iVar1 = FUN_2388_0684(local_1a);
  if (-iVar1 != -8 && -1 < -iVar1 + 8) {
    iVar2 = 8 - iVar1;
    local_4 = iVar1;
    do {
      FUN_2388_0e22(param_1,param_2,0x666);
      iVar2 = iVar2 + -1;
    } while (iVar2 != 0);
  }
  FUN_2388_0e22(param_1,param_2,local_1a);
  return;
}



void __cdecl16far
FUN_18ad_01be(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  undefined1 local_16 [20];
  
  FUN_2388_0758(0x18ad,param_3,param_4,local_16,10);
  FUN_2388_0e22(param_1,param_2,local_16);
  return;
}



void __cdecl16far
FUN_18ad_01e8(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  FUN_18ad_01be(param_1,param_2,param_3,param_4);
  FUN_2388_0e22(param_1,param_2,0x668);
  return;
}



int __cdecl16far FUN_18ad_0216(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = FUN_1d6c_0002(param_1,param_2,*(undefined2 *)0x80,*(undefined2 *)0x82);
  return iVar1 + -1;
}



int __cdecl16far FUN_18ad_0232(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = FUN_1d6c_0002(param_1,param_2,*(undefined2 *)0x3a96,*(undefined2 *)0x3a98);
  return iVar1 + -1;
}



void __cdecl16far FUN_18ad_024e(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  FUN_1d6a_0006(0);
  FUN_1d53_0008(0,param_1,param_2,*(undefined2 *)0x80,*(undefined2 *)0x82);
  return;
}



void __cdecl16far
FUN_18ad_0288(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5)

{
  undefined2 unaff_DS;
  
  FUN_1d6a_0006(param_5);
  FUN_1d53_0008(0,param_1,param_2,*(undefined2 *)0x80,*(undefined2 *)0x82);
  return;
}



int __cdecl16far
FUN_18ad_02c2(undefined2 param_1,undefined2 param_2,int param_3,undefined2 param_4,
             undefined2 param_5)

{
  int iVar1;
  undefined2 unaff_DS;
  
  FUN_1d6a_0006(param_5);
  iVar1 = FUN_1d6c_0002(param_1,param_2,*(undefined2 *)0x80,*(undefined2 *)0x82);
  FUN_1d53_0008(0,param_1,param_2,*(undefined2 *)0x80,*(undefined2 *)0x82);
  return param_3 - iVar1;
}



void __cdecl16far
FUN_18ad_0318(undefined2 param_1,undefined2 param_2,int param_3,int param_4,undefined2 param_5,
             undefined2 param_6)

{
  int iVar1;
  
  iVar1 = FUN_18ad_0216(param_1,param_2,param_5,param_6);
  param_3 = ((param_4 >> 1) - (iVar1 >> 1)) + param_3;
  if (param_3 < 0) {
    param_3 = 0;
  }
  FUN_18ad_0288(param_1,param_2,param_3);
  return;
}



void __cdecl16far FUN_18ad_035c(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  FUN_1d6a_0006(0);
  FUN_1d53_0008(0,param_1,param_2,*(undefined2 *)0x3a96,*(undefined2 *)0x3a98);
  return;
}



void __cdecl16far FUN_18ad_039a(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  FUN_1d6a_0006(0);
  FUN_1d53_0008(0,param_1,param_2,*(undefined2 *)0x3a96,*(undefined2 *)0x3a98);
  return;
}



int __cdecl16far FUN_18ad_03d2(undefined2 param_1,undefined2 param_2,int param_3)

{
  int iVar1;
  undefined2 unaff_DS;
  
  FUN_1d6a_0006(0);
  iVar1 = FUN_1d6c_0002(param_1,param_2,*(undefined2 *)0x3a96,*(undefined2 *)0x3a98);
  FUN_1d53_0008(0,param_1,param_2,*(undefined2 *)0x3a96,*(undefined2 *)0x3a98);
  return param_3 - iVar1;
}



void __cdecl16far
FUN_18ad_0430(undefined2 param_1,undefined2 param_2,int param_3,int param_4,undefined2 param_5,
             undefined2 param_6,undefined2 param_7)

{
  int iVar1;
  
  iVar1 = FUN_18ad_0232(param_1,param_2,param_5,param_6,param_7);
  param_3 = ((param_4 >> 1) - (iVar1 >> 1)) + param_3;
  if (param_3 < 0) {
    param_3 = 0;
  }
  FUN_18ad_039a(param_1,param_2,param_3);
  return;
}



void __cdecl16far FUN_18ad_0478(undefined2 param_1,int param_2)

{
  undefined2 unaff_DS;
  undefined2 uVar1;
  
  if (param_2 < 0) {
    uVar1 = *(undefined2 *)0x528a;
  }
  else {
    uVar1 = *(undefined2 *)(param_2 * 0x10 + 0x4ee6);
  }
  FUN_18ad_00e2(param_1,uVar1);
  if ((7 < param_2) && (param_2 < 0x18)) {
    FUN_18a2_00b0(param_1);
    FUN_18ad_00e2(param_1,*(undefined2 *)0x4b5a);
  }
  return;
}



uint __cdecl16far FUN_18f9_0b0e(void)

{
  uint uVar1;
  int iVar2;
  uint in_DX;
  int iVar3;
  undefined2 unaff_DS;
  undefined2 local_16;
  
  local_16 = 0;
  uVar1 = FUN_130b_0006(0xc);
  in_DX = in_DX | uVar1;
  if (in_DX == 0) {
    *(undefined2 *)0x62 = 0x321;
  }
  else {
    iVar2 = FUN_1ddb_0008();
    if (in_DX == 0 && iVar2 == 0) {
      *(undefined2 *)0x62 = 0x322;
    }
    else {
      iVar3 = 0;
      do {
        FUN_1d8f_0000(0,iVar2,in_DX);
        iVar3 = iVar3 + 1;
      } while (iVar3 < 0xc);
      FUN_1cc9_0310(iVar2,in_DX);
      local_16 = uVar1;
    }
  }
  return local_16;
}



bool __cdecl16far FUN_18f9_0bc2(void)

{
  undefined2 uVar1;
  int in_DX;
  undefined2 unaff_DS;
  
  uVar1 = FUN_18f9_0b0e(0x6ac);
  *(undefined2 *)0x4b8 = uVar1;
  *(int *)0x4ba = in_DX;
  return in_DX == 0 && *(int *)0x4b8 == 0;
}



byte __cdecl16far FUN_19b7_0006(byte param_1)

{
  uint unaff_1000000c;
  
  if ((param_1 & 0x20) != 0) {
    return (-((unaff_1000000c & 0x80) == 0) & 1U) + 0x1b;
  }
  return param_1 & 0x1f;
}



undefined2 __cdecl16far FUN_19b7_0032(undefined2 param_1,undefined2 param_2)

{
  undefined1 uVar1;
  int iVar2;
  undefined2 uVar3;
  
  uVar3 = 0x19;
  iVar2 = FUN_12ab_000e(param_1,param_2);
  if (iVar2 != 0) {
    uVar1 = FUN_12ab_0112(param_1,param_2);
    uVar3 = FUN_19b7_0006(uVar1);
  }
  return uVar3;
}



undefined2 __cdecl16far FUN_19b7_006c(undefined2 param_1,undefined2 param_2)

{
  uint uVar1;
  
  uVar1 = FUN_12ab_0112(param_1,param_2);
  if (((uVar1 & 0x1f) != 0x19) && ((uVar1 & 0x1f) != 0x1a)) {
    return 0;
  }
  return 1;
}



undefined2 __cdecl16far FUN_19b7_009a(undefined2 param_1,undefined2 param_2)

{
  uint uVar1;
  
  uVar1 = FUN_12ab_0112(param_1,param_2);
  uVar1 = uVar1 & 0x1f;
  if (((uVar1 < 8) || (0xf < uVar1)) && ((uVar1 < 0x10 || (0x17 < uVar1)))) {
    return 0;
  }
  return 1;
}



uint __cdecl16far FUN_19c4_0002(void)

{
  int *piVar1;
  int iVar2;
  undefined2 uVar3;
  bool bVar4;
  uint *puVar5;
  uint uVar6;
  int iVar7;
  undefined2 unaff_DS;
  uint local_30;
  undefined4 local_2a;
  uint *local_22;
  int local_1e;
  int local_1c;
  uint local_1a;
  int local_18;
  int local_16;
  int local_10;
  uint local_e;
  uint local_c;
  int local_a;
  int local_4;
  
  local_c = 0;
  iVar2 = *(int *)0x8c;
  uVar3 = *(undefined2 *)0x8e;
  puVar5 = (uint *)CONCAT11((char)((uint)iVar2 >> 8) + -0x80,(char)iVar2);
  FUN_1c49_000e(*(undefined2 *)0x5eba,*(undefined2 *)0x5ebc,*(undefined2 *)0x5ebe,
                *(undefined2 *)0x5ec0);
  local_1e = 1;
  do {
    if (local_1e < 0) {
      local_18 = 0;
      do {
        *(undefined2 *)(local_18 * 2 + 0x5270) = *(undefined2 *)(local_18 * 2 + iVar2);
        local_18 = local_18 + 1;
      } while (local_18 < 0x10);
      FUN_1000_0002();
      return local_c;
    }
    FUN_2388_0e68(iVar2,uVar3,0,0x8000);
    FUN_2388_0e68(puVar5,uVar3,0,*(int *)0x4b14 * *(int *)0x4b12 * 2);
    local_30 = 0;
    local_e = 0;
    bVar4 = false;
    for (local_1c = 1; local_1c < *(int *)0x4b14 + -1; local_1c = local_1c + 1) {
      for (local_16 = *(int *)0x4b12 + -2; 0 < local_16; local_16 = local_16 + -1) {
        iVar7 = FUN_12ab_000e(local_16,local_1c);
        if (iVar7 == 0) {
LAB_19c4_01ce:
          bVar4 = false;
          local_e = 0;
        }
        else {
          iVar7 = FUN_19b7_006c(local_16,local_1c);
          if (iVar7 != local_1e) goto LAB_19c4_01ce;
          for (local_10 = -1; local_10 < 2; local_10 = local_10 + 1) {
            local_1a = puVar5[(local_1c + -1) * *(int *)0x4b12 + local_16 + local_10];
            if (local_1a != 0) {
              if ((local_e != 0) && (local_1a != local_e)) {
                uVar6 = local_1a;
                if ((int)local_1a < (int)local_e) {
                  uVar6 = local_e;
                }
                if ((int)local_e < (int)local_1a) {
                  local_1a = local_e;
                }
                piVar1 = (int *)(local_1a * 2 + iVar2);
                *piVar1 = *piVar1 + *(int *)(uVar6 * 2 + iVar2);
                *(undefined2 *)(uVar6 * 2 + iVar2) = 0;
                for (local_a = 1; local_a <= local_1c; local_a = local_a + 1) {
                  for (local_4 = 1; local_4 <= *(int *)0x4b12; local_4 = local_4 + 1) {
                    if (puVar5[*(int *)0x4b12 * local_a + local_4] == uVar6) {
                      puVar5[*(int *)0x4b12 * local_a + local_4] = local_1a;
                    }
                  }
                }
              }
              local_e = local_1a;
            }
          }
          if (local_e == 0) {
            if (!bVar4) {
              local_30 = 0;
              if (((local_1c == 1) || (*(int *)0x4b14 - local_1c == 2)) && (local_1e == 0)) {
                local_30 = 0x10;
              }
              do {
                local_30 = local_30 + 1;
                if (*(int *)(local_30 * 2 + iVar2) == 0) break;
              } while ((int)local_30 < 0x4002);
              if (0x3fff < (int)local_30) {
                local_30 = 0x3fff;
                FUN_2388_05a8(0x700);
                FUN_1baf_0018();
              }
            }
            local_e = local_30;
          }
          puVar5[*(int *)0x4b12 * local_1c + local_16] = local_e;
          piVar1 = (int *)(local_e * 2 + iVar2);
          *piVar1 = *piVar1 + 1;
          bVar4 = true;
        }
      }
      FUN_1000_0006();
    }
    local_22 = (uint *)CONCAT22(uVar3,puVar5);
    local_2a = (undefined1 *)CONCAT22(*(undefined2 *)0x4b2,(undefined1 *)*(undefined2 *)0x4b0);
    for (local_1c = 0; local_1c < *(int *)0x4b14; local_1c = local_1c + 1) {
      for (local_16 = 0; local_16 < *(int *)0x4b12; local_16 = local_16 + 1) {
        if (*local_22 != 0) {
          if (0xf < *local_22) {
            iVar7 = *(int *)(*local_22 * 2 + iVar2);
            if (iVar7 < 1) {
              if (iVar7 < 1) {
                local_30 = ~*(uint *)(*local_22 * 2 + iVar2) + 1;
              }
              else {
                local_30 = *(uint *)(*local_22 * 2 + iVar2);
              }
            }
            else {
              local_30 = 0;
              do {
                local_30 = local_30 + 1;
              } while (*(int *)(local_30 * 2 + iVar2) != 0);
              if (0xf < (int)local_30) {
                local_c = local_c | 1 << ((byte)local_1e & 0x1f);
                *local_22 = 0xf;
                goto LAB_19c4_0283;
              }
              uVar6 = *local_22;
              *(undefined2 *)(local_30 * 2 + iVar2) = *(undefined2 *)(uVar6 * 2 + iVar2);
              *(int *)(uVar6 * 2 + iVar2) = -local_30;
            }
            *local_22 = local_30;
          }
LAB_19c4_0283:
          *local_2a = (char)*local_22;
        }
        local_22 = (uint *)CONCAT22(local_22._2_2_,(uint *)local_22 + 1);
        local_2a = (undefined1 *)CONCAT22(local_2a._2_2_,(undefined1 *)local_2a + 1);
      }
      FUN_1000_0006();
    }
    local_1e = local_1e + -1;
  } while( true );
}



void __cdecl16near FUN_19f9_000a(void)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  
  uVar1 = *(undefined2 *)0x4b12;
  *(undefined2 *)0x5aa4 = uVar1;
  *(undefined2 *)0x5ebc = uVar1;
  *(undefined2 *)0x4a6e = uVar1;
  *(undefined2 *)0x4e94 = uVar1;
  uVar1 = *(undefined2 *)0x4b14;
  *(undefined2 *)0x5aa2 = uVar1;
  *(undefined2 *)0x5eba = uVar1;
  *(undefined2 *)0x4a6c = uVar1;
  *(undefined2 *)0x4e92 = uVar1;
  uVar1 = *(undefined2 *)0x4aa;
  *(undefined2 *)0x4e96 = *(undefined2 *)0x4a8;
  *(undefined2 *)0x4e98 = uVar1;
  uVar1 = *(undefined2 *)0x4ae;
  *(undefined2 *)0x4a70 = *(undefined2 *)0x4ac;
  *(undefined2 *)0x4a72 = uVar1;
  uVar1 = *(undefined2 *)0x4b2;
  *(undefined2 *)0x5ebe = *(undefined2 *)0x4b0;
  *(undefined2 *)0x5ec0 = uVar1;
  uVar1 = *(undefined2 *)0x4b6;
  *(undefined2 *)0x5aa6 = *(undefined2 *)0x4b4;
  *(undefined2 *)0x5aa8 = uVar1;
  return;
}



undefined2 __cdecl16far FUN_19f9_0062(void)

{
  int iVar1;
  int in_AX;
  undefined2 uVar2;
  int iVar3;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 1;
  if (*(int *)0x4a6 == 0) {
    iVar3 = *(int *)0x4b14;
    iVar1 = *(int *)0x4b12;
    *(int *)0x4cc = iVar3 * iVar1;
    *(int *)0x4ce = iVar3 * iVar1 >> 0xf;
  }
  else {
    *(undefined2 *)0x4cc = 0x2eb4;
    *(undefined2 *)0x4ce = 0;
  }
  if (in_AX != 0) {
    *(undefined2 *)0x4cc = 12000;
    *(undefined2 *)0x4ce = 0;
  }
  *(undefined2 *)0x64ec = 0xf0;
  *(undefined2 *)0x64ee = 0xc0;
  iVar3 = *(int *)0x4ce;
  uVar2 = FUN_1cc9_02e2();
  *(undefined2 *)0x4a8 = uVar2;
  *(int *)0x4aa = iVar3;
  if (iVar3 != 0 || *(int *)0x4a8 != 0) {
    iVar3 = *(int *)0x4ce;
    uVar2 = FUN_1cc9_02e2();
    *(undefined2 *)0x4ac = uVar2;
    *(int *)0x4ae = iVar3;
    if (iVar3 != 0 || *(int *)0x4ac != 0) {
      iVar3 = *(int *)0x4ce;
      uVar2 = FUN_1cc9_02e2();
      *(undefined2 *)0x4b0 = uVar2;
      *(int *)0x4b2 = iVar3;
      if (iVar3 != 0 || *(int *)0x4b0 != 0) {
        iVar3 = *(int *)0x4ce;
        uVar2 = FUN_1cc9_02e2();
        *(undefined2 *)0x4b4 = uVar2;
        *(int *)0x4b6 = iVar3;
        if (iVar3 != 0 || *(int *)0x4b4 != 0) {
          FUN_19f9_000a();
          local_4 = 0;
        }
      }
    }
  }
  return local_4;
}



undefined2 __cdecl16far FUN_19f9_0128(void)

{
  return 1;
}



undefined2 __cdecl16far FUN_19f9_012c(void)

{
  undefined2 unaff_DS;
  
  if ((*(int *)0x4e9e < 1) && ((*(int *)0x4e9e < 0 || (*(uint *)0x4e9c < 0x2ee1)))) {
    *(undefined2 *)0x4a6 = 0;
  }
  else {
    *(undefined2 *)0x4a6 = 1;
  }
  if (*(int *)0x4a6 != 0) {
    *(undefined2 *)0x4a4 = 9999;
    return 1;
  }
  return 0;
}



undefined2 __cdecl16far FUN_19f9_0170(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 unaff_DS;
  long lVar4;
  int local_6;
  undefined2 local_4;
  
  local_4 = 1;
  FUN_1c04_005c(0x4a0,unaff_DS,0x4a18,unaff_DS,0x4a18,unaff_DS);
  iVar2 = FUN_1297_0104(0x4a18,unaff_DS);
  if (iVar2 == 0) {
    *(undefined2 *)0x4a4 = 1;
  }
  else {
    iVar3 = FUN_2388_03be(0x4b12,4,1,iVar2);
    if ((iVar3 == 0) || (iVar3 = FUN_2388_03be(&local_6,2,1,iVar2), iVar3 == 0)) {
      *(undefined2 *)0x4a4 = 2;
    }
    else if (((local_6 < 5) && (3 < local_6)) || (*(int *)0x49e < 0)) {
      *(int *)0x49e = local_6;
      iVar3 = *(int *)0x4b14;
      iVar1 = *(int *)0x4b12;
      *(undefined2 *)0x4e9c = (int)((long)iVar3 * (long)iVar1);
      *(undefined2 *)0x4e9e = (int)((ulong)((long)iVar3 * (long)iVar1) >> 0x10);
      iVar3 = FUN_19f9_012c();
      if (iVar3 == 0) {
        if (*(int *)0x4a6 == 0) {
          lVar4 = FUN_1bca_0000(1,0,*(undefined2 *)0x4a8,*(undefined2 *)0x4aa);
          if (lVar4 == 0) {
            *(undefined2 *)0x4a4 = 4;
            goto LAB_19f9_029d;
          }
          lVar4 = FUN_1bca_0000(1,0,*(undefined2 *)0x4ac,*(undefined2 *)0x4ae);
          if (lVar4 == 0) {
            *(undefined2 *)0x4a4 = 5;
            goto LAB_19f9_029d;
          }
          lVar4 = FUN_1bca_0000(1,0,*(undefined2 *)0x4b0,*(undefined2 *)0x4b2);
          if (lVar4 == 0) {
            *(undefined2 *)0x4a4 = 6;
            goto LAB_19f9_029d;
          }
        }
        local_4 = 0;
        *(undefined2 *)0x4a4 = 0;
        FUN_19f9_000a();
      }
    }
    else {
      *(undefined2 *)0x4a4 = 3;
    }
  }
LAB_19f9_029d:
  if (iVar2 != 0) {
    FUN_2388_02c2(iVar2);
  }
  return local_4;
}



undefined2 __cdecl16far FUN_19f9_02b0(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 unaff_DS;
  long lVar4;
  undefined2 local_6;
  undefined2 local_4;
  
  local_4 = 1;
  FUN_1c04_005c(0x4a0,unaff_DS,0x4a18,unaff_DS,0x4a18,unaff_DS);
  iVar2 = FUN_1297_0104(0x4a18,unaff_DS);
  if (iVar2 == 0) {
    *(undefined2 *)0x4a4 = 1;
    goto LAB_19f9_03a7;
  }
  iVar3 = FUN_2388_04a2(0x4b12,4,1,iVar2);
  if (iVar3 != 0) {
    local_6 = *(undefined2 *)0x49e;
    iVar3 = FUN_2388_04a2(&local_6,2,1,iVar2);
    if (iVar3 != 0) {
      iVar3 = *(int *)0x4b14;
      iVar1 = *(int *)0x4b12;
      *(undefined2 *)0x4e9c = (int)((long)iVar3 * (long)iVar1);
      *(undefined2 *)0x4e9e = (int)((ulong)((long)iVar3 * (long)iVar1) >> 0x10);
      if (*(int *)0x4a6 == 0) {
        lVar4 = FUN_1bea_0008(1,0,*(undefined2 *)0x4a8,*(undefined2 *)0x4aa);
        if (lVar4 == 0) {
          *(undefined2 *)0x4a4 = 4;
          goto LAB_19f9_03a7;
        }
        lVar4 = FUN_1bea_0008(1,0,*(undefined2 *)0x4ac,*(undefined2 *)0x4ae);
        if (lVar4 != 0) {
          lVar4 = FUN_1bea_0008(1,0,*(undefined2 *)0x4b0,*(undefined2 *)0x4b2);
          if (lVar4 != 0) goto LAB_19f9_039f;
        }
        *(undefined2 *)0x4a4 = 5;
      }
      else {
LAB_19f9_039f:
        local_4 = 0;
        *(undefined2 *)0x4a4 = 0;
      }
      goto LAB_19f9_03a7;
    }
  }
  *(undefined2 *)0x4a4 = 2;
LAB_19f9_03a7:
  if (iVar2 != 0) {
    FUN_2388_02c2(iVar2);
  }
  return local_4;
}



undefined2 __cdecl16far FUN_19f9_03ba(int param_1,int param_2)

{
  int iVar1;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 1;
  *(int *)0x4b12 = param_1;
  *(int *)0x4b14 = param_2;
  *(undefined2 *)0x4e9c = (int)((long)param_2 * (long)param_1);
  *(undefined2 *)0x4e9e = (int)((ulong)((long)param_2 * (long)param_1) >> 0x10);
  iVar1 = FUN_19f9_012c();
  if (iVar1 == 0) {
    if (*(int *)0x4a6 == 0) {
      FUN_2388_0e68(*(undefined2 *)0x4a8,*(undefined2 *)0x4aa,0x19,*(undefined2 *)0x4e9c);
      FUN_2388_0e68(*(undefined2 *)0x4ac,*(undefined2 *)0x4ae,0,*(undefined2 *)0x4e9c);
      FUN_2388_0e68(*(undefined2 *)0x4b0,*(undefined2 *)0x4b2,0,*(undefined2 *)0x4e9c);
    }
    local_4 = 0;
    *(undefined2 *)0x4a4 = 0;
    *(undefined2 *)0x49e = 4;
  }
  return local_4;
}



undefined2 __cdecl16far FUN_19f9_043e(void)

{
  int iVar1;
  int iVar2;
  undefined2 unaff_DS;
  undefined2 local_6;
  undefined2 local_4;
  
  local_4 = 1;
  local_6 = 0;
  if (*(int *)0x4d8 == 0) {
    FUN_1c04_005c(0x4a0,unaff_DS,0x4a18,unaff_DS,0x4a18,unaff_DS);
    local_6 = FUN_1297_0104(0x4a18,unaff_DS);
    *(undefined2 *)0x4b12 = 0x78;
    *(undefined2 *)0x4b14 = 0x4b;
    *(undefined2 *)0x4e9c = 9000;
    *(undefined2 *)0x4e9e = 0;
    if (local_6 != 0) {
      iVar2 = FUN_2388_03be(0x4b12,4,1,local_6);
      if (iVar2 != 0) {
        iVar2 = *(int *)0x4b14;
        iVar1 = *(int *)0x4b12;
        *(undefined2 *)0x4e9c = (int)((long)iVar2 * (long)iVar1);
        *(undefined2 *)0x4e9e = (int)((ulong)((long)iVar2 * (long)iVar1) >> 0x10);
      }
    }
  }
  iVar2 = FUN_19f9_012c();
  if (iVar2 == 0) {
    iVar2 = FUN_19f9_0062();
    if (iVar2 == 0) {
      local_4 = 0;
    }
    else {
      *(undefined2 *)0x4a4 = 0x13;
    }
  }
  if (local_6 != 0) {
    FUN_2388_02c2(local_6);
  }
  return local_4;
}



void __cdecl16far FUN_1a47_0006(void)

{
  byte bVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  undefined2 unaff_DS;
  
  bVar1 = *(byte *)0x4d0;
  *(int *)0x52d4 = 0xf << (bVar1 & 0x1f);
  *(int *)0x52d8 = 0xc << (bVar1 & 0x1f);
  if (*(int *)0x4d6 != 0) {
    *(undefined2 *)0x52d4 = 5;
    *(undefined2 *)0x52d8 = 5;
    *(undefined2 *)0x4d0 = 0;
  }
  iVar2 = 0x10 >> (*(byte *)0x4d0 & 0x1f);
  *(int *)0x4e8a = iVar2;
  *(int *)0x4e8c = iVar2;
  iVar2 = -((*(int *)0x52d4 >> 1) - *(int *)0x4c8);
  *(int *)0x49f2 = iVar2;
  iVar3 = -((*(int *)0x52d8 >> 1) - *(int *)0x4ca);
  *(int *)0x49f4 = iVar3;
  if (*(int *)0x4d6 == 0) {
    if (iVar2 < 1) {
      iVar2 = 1;
    }
    iVar4 = (*(int *)0x4b12 - *(int *)0x52d4) + -1;
    if (iVar4 < iVar2) {
      iVar2 = iVar4;
    }
    *(int *)0x49f2 = iVar2;
    if (iVar3 < 1) {
      iVar3 = 1;
    }
    iVar2 = (*(int *)0x4b14 - *(int *)0x52d8) + -1;
    if (iVar2 < iVar3) {
      iVar3 = iVar2;
    }
    *(int *)0x49f4 = iVar3;
  }
  *(undefined2 *)0x5ad8 = 0;
  *(undefined2 *)0x5af4 = 0;
  iVar2 = *(int *)0x4b12;
  if (iVar2 + -2 < *(int *)0x52d4) {
    *(undefined2 *)0x49f2 = 1;
    *(int *)0x5ad8 = (*(int *)0x52d4 - *(int *)0x4b12) + 2 >> 1;
    *(int *)0x52d4 = iVar2 + -2;
  }
  iVar2 = *(int *)0x4b14;
  if (iVar2 + -2 < *(int *)0x52d8) {
    *(undefined2 *)0x49f4 = 1;
    *(int *)0x5af4 = (*(int *)0x52d8 - *(int *)0x4b14) + 2 >> 1;
    *(int *)0x52d8 = iVar2 + -2;
  }
  *(int *)0x5af6 = *(int *)0x49f2 + -1;
  *(int *)0x5af8 = *(int *)0x49f4 + -1;
  if (*(int *)0x4a6 == 0) {
    *(undefined2 *)0x4e70 = *(undefined2 *)0x4b12;
    iVar2 = *(int *)0x4b14;
  }
  else if (*(int *)0x4d6 == 0) {
    bVar1 = *(byte *)0x4d0;
    *(int *)0x4e70 = (0xf << (bVar1 & 0x1f)) + 2;
    iVar2 = (0xc << (bVar1 & 0x1f)) + 2;
  }
  else {
    iVar2 = *(int *)0x5af8;
    iVar3 = *(int *)0x4b12 + -1;
    if (*(int *)0x5af6 + 6 < *(int *)0x4b12 + -1) {
      iVar3 = *(int *)0x5af6 + 6;
    }
    iVar4 = *(int *)0x5af6;
    if (iVar4 < 0) {
      iVar4 = 0;
    }
    *(int *)0x5af6 = iVar4;
    *(int *)0x4e70 = (iVar3 - iVar4) + 1;
    iVar3 = *(int *)0x4b14 + -1;
    if (iVar2 + 6 < *(int *)0x4b14 + -1) {
      iVar3 = iVar2 + 6;
    }
    iVar2 = *(int *)0x5af8;
    if (iVar2 < 0) {
      iVar2 = 0;
    }
    *(int *)0x5af8 = iVar2;
    iVar2 = (iVar3 - iVar2) + 1;
  }
  *(int *)0x4e76 = iVar2;
  bVar1 = *(byte *)0x4d0;
  *(int *)0x4d2 = 100 >> (bVar1 & 0x1f);
  *(int *)0x4d4 = (5 << (bVar1 & 0x1f)) + 5;
  *(int *)0x6026 = *(int *)0x52d4 + *(int *)0x49f2 + -1;
  *(int *)0x603e = *(int *)0x52d8 + *(int *)0x49f4 + -1;
  return;
}



undefined1 __cdecl16near FUN_1a47_01ae(void)

{
  byte *pbVar1;
  byte bVar2;
  char cVar3;
  undefined1 uVar4;
  int iVar5;
  int iVar6;
  undefined2 unaff_DS;
  byte local_c;
  uint local_6;
  
  *(undefined1 *)0x6974 = 0;
  *(undefined1 *)0x5ad3 = 0;
  local_6 = 0;
  do {
    *(undefined1 *)(local_6 + 0x499a) = 0;
    local_6 = local_6 + 1;
  } while ((int)local_6 < 4);
  if (*(int *)0x4d0 == 0) {
    for (local_6 = 0; (int)local_6 < 8; local_6 = local_6 + 1) {
      if (*(char *)(local_6 + 0x6ca) < '\0') {
        iVar5 = -*(int *)0x4e70;
      }
      else {
        iVar5 = 0;
      }
      if (*(char *)(local_6 + 0x6ca) < '\x01') {
        iVar6 = 0;
      }
      else {
        iVar6 = *(int *)0x4e70;
      }
      bVar2 = *(byte *)((int)*(char *)(local_6 + 0x6c0) + *(int *)0x5afa + iVar5 + iVar6);
      local_c = bVar2 & 0x1f;
      if (local_c < 0x18) {
        local_c = bVar2 & 7;
      }
      cVar3 = FUN_12ab_05bc(local_c);
      if ((cVar3 != '\x19') && (cVar3 != '\x1a')) {
        *(byte *)0x5ad3 = *(byte *)0x5ad3 | '\x01' << ((byte)local_6 & 0x1f);
        *(char *)0x6974 = *(char *)0x6974 + '\x01';
        if ((local_6 & 1) == 0) {
          *(byte *)0x4e84 = local_c;
          pbVar1 = (byte *)(((int)local_6 >> 1) + 0x499a);
          *pbVar1 = *pbVar1 | 4;
          pbVar1 = (byte *)(((byte)((char)((int)local_6 >> 1) + 1) & 3) + 0x499a);
          *pbVar1 = *pbVar1 | 1;
        }
        else {
          pbVar1 = (byte *)(((int)((byte)((byte)local_6 + 1) & 6) >> 1) + 0x499a);
          *pbVar1 = *pbVar1 | 2;
        }
      }
    }
  }
  uVar4 = FUN_12ab_05bc(*(undefined1 *)0x4e84);
  *(undefined1 *)0x5acc = uVar4;
  return *(undefined1 *)0x6974;
}



int __cdecl16near FUN_1a47_030e(void)

{
  int iVar1;
  byte in_AL;
  int in_DX;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  if (*(int *)0x4d0 <= in_DX) {
    uVar2 = (undefined2)((ulong)*(undefined4 *)0x5afa >> 0x10);
    if ((in_AL & *(byte *)((int)*(undefined4 *)0x5afa - *(int *)0x4e70)) != 0) {
      local_4 = 8;
    }
    iVar1 = *(int *)0x5afa;
    if ((in_AL & *(byte *)(iVar1 + *(int *)0x4e70)) != 0) {
      local_4 = local_4 + 4;
    }
    if ((in_AL & *(byte *)(iVar1 + -1)) != 0) {
      local_4 = local_4 + 2;
    }
    if ((in_AL & *(byte *)(iVar1 + 1)) != 0) {
      local_4 = local_4 + 1;
    }
  }
  return local_4;
}



int __cdecl16near FUN_1a47_036e(void)

{
  int iVar1;
  uint in_AX;
  int in_DX;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined2 local_6;
  
  local_6 = 0;
  if (*(int *)0x4d0 <= in_DX) {
    uVar2 = (undefined2)((ulong)*(undefined4 *)0x5afa >> 0x10);
    if ((*(byte *)((int)*(undefined4 *)0x5afa - *(int *)0x4e70) & 0xa0) == in_AX) {
      local_6 = 8;
    }
    iVar1 = *(int *)0x5afa;
    if ((*(byte *)(iVar1 + *(int *)0x4e70) & 0xa0) == in_AX) {
      local_6 = local_6 + 4;
    }
    if ((*(byte *)(iVar1 + -1) & 0xa0) == in_AX) {
      local_6 = local_6 + 2;
    }
    if ((*(byte *)(iVar1 + 1) & 0xa0) == in_AX) {
      local_6 = local_6 + 1;
    }
  }
  return local_6;
}



undefined2 __cdecl16near FUN_1a47_03de(undefined2 param_1,int param_2)

{
  byte bVar1;
  byte bVar2;
  undefined2 unaff_DS;
  undefined2 local_6;
  
  local_6 = 0;
  bVar1 = *(byte *)((int)*(undefined4 *)0x5afa + param_2);
  bVar2 = bVar1 & 0x1f;
  if (((bVar2 < 0x18) && ((bVar1 & 7) != 1)) && (7 < bVar2)) {
    local_6 = 1;
  }
  return local_6;
}



int __cdecl16near FUN_1a47_0418(void)

{
  undefined2 in_AX;
  int iVar1;
  int in_DX;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  if (*(int *)0x4d0 <= in_DX) {
    iVar1 = FUN_1a47_03de();
    if (iVar1 != 0) {
      local_4 = 8;
    }
    iVar1 = FUN_1a47_03de(in_AX,*(undefined2 *)0x4e70);
    if (iVar1 != 0) {
      local_4 = local_4 + 4;
    }
    iVar1 = FUN_1a47_03de(in_AX,0xffff);
    if (iVar1 != 0) {
      local_4 = local_4 + 2;
    }
    iVar1 = FUN_1a47_03de(in_AX,1);
    if (iVar1 != 0) {
      local_4 = local_4 + 1;
    }
  }
  return local_4;
}



int __cdecl16near FUN_1a47_047e(void)

{
  int iVar1;
  byte in_AL;
  int in_DX;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  if (*(int *)0x4d0 <= in_DX) {
    uVar2 = (undefined2)((ulong)*(undefined4 *)0x4a7e >> 0x10);
    if ((in_AL & *(byte *)((int)*(undefined4 *)0x4a7e - *(int *)0x4e70)) != 0) {
      local_4 = 8;
    }
    iVar1 = *(int *)0x4a7e;
    if ((in_AL & *(byte *)(iVar1 + *(int *)0x4e70)) != 0) {
      local_4 = local_4 + 4;
    }
    if ((in_AL & *(byte *)(iVar1 + -1)) != 0) {
      local_4 = local_4 + 2;
    }
    if ((in_AL & *(byte *)(iVar1 + 1)) != 0) {
      local_4 = local_4 + 1;
    }
  }
  return local_4;
}



uint __cdecl16near FUN_1a47_04de(void)

{
  byte in_AL;
  int iVar1;
  int in_DX;
  undefined2 unaff_DS;
  undefined2 local_8;
  undefined2 local_6;
  undefined2 local_4;
  
  local_6 = 0;
  if (*(int *)0x4d0 <= in_DX) {
    local_8 = 1;
    for (local_4 = 0; local_4 < 8; local_4 = local_4 + 1) {
      if (*(char *)(local_4 + 0x6ca) == '\0') {
        iVar1 = 0;
      }
      else if (*(char *)(local_4 + 0x6ca) < '\0') {
        iVar1 = ~*(uint *)0x4e70 + 1;
      }
      else {
        iVar1 = *(int *)0x4e70;
      }
      if ((in_AL & *(byte *)((int)*(char *)(local_4 + 0x6c0) + *(int *)0x4a7e + iVar1)) != 0) {
        local_6 = local_6 | local_8;
      }
      local_8 = local_8 << 1;
    }
  }
  return local_6;
}



void __cdecl16near FUN_1a47_0552(void)

{
  undefined2 unaff_DS;
  
  if (99 < *(int *)0x4d2) {
    FUN_1d8f_0000((uint)*(byte *)0x715 + *(int *)0x4b16 + -0xf,*(undefined2 *)0x4c0,
                  *(undefined2 *)0x4c2);
    return;
  }
  FUN_1dae_000a(*(undefined2 *)0x4d2,*(undefined2 *)0x4b16,*(undefined2 *)0x4c0,*(undefined2 *)0x4c2
               );
  return;
}



void __cdecl16near FUN_1a47_05b2(void)

{
  undefined2 unaff_DS;
  
  if (*(int *)0x4d0 == 0) {
    FUN_130b_0048(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba);
    return;
  }
  FUN_130b_011e(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba);
  return;
}



void __cdecl16near FUN_1a47_0616(void)

{
  undefined2 unaff_DS;
  
  if (99 < *(int *)0x4d2) {
    FUN_1e71_000c((uint)*(byte *)0x715 + *(int *)0x4b16 + -0xf,*(undefined2 *)0x4c0,
                  *(undefined2 *)0x4c2);
    return;
  }
  FUN_1e92_000a(*(undefined2 *)0x4d2,*(undefined2 *)0x4b16,*(undefined2 *)0x4c0,*(undefined2 *)0x4c2
               );
  return;
}



void __cdecl16near FUN_1a47_0676(void)

{
  undefined2 unaff_DS;
  
  if (*(int *)0x4d0 == 0) {
    FUN_130b_00ac(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba);
    return;
  }
  FUN_130b_01d4(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba);
  return;
}



void __cdecl16near FUN_1a47_06da(int param_1,int param_2,int param_3)

{
  undefined2 uVar1;
  byte bVar2;
  char cVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  uint uVar8;
  int iVar9;
  undefined2 unaff_DS;
  bool bVar10;
  uint local_24;
  char local_1e;
  int local_14;
  uint local_12;
  byte local_e;
  int local_6;
  
  uVar1 = *(undefined2 *)0x4e90;
  *(undefined2 *)0x4e90 = 0;
  local_6 = 0;
  do {
    if (3 < local_6) {
      *(undefined2 *)0x4e90 = uVar1;
      return;
    }
    iVar5 = (int)*(char *)(local_6 + 0x6ba);
    local_14 = (int)*(char *)(local_6 + 0x6b4);
    iVar6 = local_14 + *(int *)0x5f16;
    iVar9 = iVar5 + *(int *)0x634c;
    iVar7 = FUN_12ab_000e(iVar6,iVar9);
    bVar10 = iVar7 == 0;
    if (*(int *)0x4d6 != 0) {
      iVar7 = iVar6 - *(int *)0x4c8;
      if (iVar7 < 1) {
        iVar7 = ~(iVar6 - *(int *)0x4c8) + 1;
      }
      iVar4 = iVar9 - *(int *)0x4ca;
      if (iVar4 < 1) {
        iVar4 = ~(iVar9 - *(int *)0x4ca) + 1;
      }
      iVar7 = FUN_12ab_0040(iVar7,iVar4,*(undefined2 *)0x4d6);
      bVar10 = bVar10 || iVar7 == 0;
    }
    if (iVar5 < 0) {
      local_14 = local_14 - *(int *)0x4e70;
    }
    if (0 < iVar5) {
      local_14 = local_14 + *(int *)0x4e70;
    }
    bVar2 = *(byte *)((int)*(undefined4 *)0x5afa + local_14);
    local_e = bVar2 & 0x1f;
    if (local_e < 0x18) {
      local_e = bVar2 & 7;
    }
    local_1e = FUN_12ab_05bc(local_e);
    if (((*(char *)0x5aaa != '\0') &&
        ((*(byte *)0x5aaa & *(byte *)((int)*(undefined4 *)0x52da + local_14)) == 0)) || (bVar10)) {
      bVar10 = true;
    }
    else {
      bVar10 = false;
    }
    if ((param_1 == 0) || (!bVar10)) {
      if (((local_1e == '\x19') || (local_1e == '\x1a')) && (param_2 == 0)) {
        for (local_12 = 7; ((local_1e == '\x19' || (local_1e == '\x1a')) && (-1 < (int)local_12));
            local_12 = local_12 - 1) {
          iVar5 = *(char *)(local_12 + 0x6c0) + iVar6;
          iVar7 = *(char *)(local_12 + 0x6ca) + iVar9;
          if (((((local_12 & 1) == 0) && (-1 < iVar5)) && (-1 < iVar7)) &&
             ((iVar5 < *(int *)0x4b12 && (iVar7 < *(int *)0x4b14)))) {
            uVar8 = FUN_12ab_0112(iVar5,iVar7);
            local_24 = uVar8 & 0x1f;
            if (local_24 < 0x18) {
              local_24 = uVar8 & 7;
            }
            local_1e = FUN_12ab_05bc(local_24);
          }
        }
        if ((local_1e == '\x19') || (local_1e == '\x1a')) goto LAB_1a47_07ad;
      }
      if (((local_1e != '\x19') && (local_1e != '\x1a')) ||
         (((param_3 != 0 || (param_1 != 0)) || (bVar10)))) {
        bVar2 = *(byte *)0x5acc & 0x1f;
        if (bVar2 < 0x18) {
          bVar2 = *(byte *)0x5acc & 7;
        }
        cVar3 = FUN_12ab_05bc(bVar2);
        if (((cVar3 != local_1e) || (param_1 != 0)) || (bVar10)) {
          FUN_1a47_0552();
          FUN_1a47_0676();
        }
      }
    }
LAB_1a47_07ad:
    local_6 = local_6 + 1;
  } while( true );
}



void __cdecl16near FUN_1a47_0932(void)

{
  byte bVar1;
  bool bVar2;
  undefined1 uVar3;
  byte bVar4;
  char cVar5;
  int in_AX;
  int iVar6;
  uint uVar7;
  int iVar8;
  undefined2 unaff_DS;
  undefined2 uVar9;
  undefined2 uVar10;
  undefined2 local_24;
  int local_1e;
  uint local_1c;
  int local_18;
  int local_e;
  int local_8;
  char local_6;
  
  local_1e = 0;
  *(undefined1 *)0x4c6a = *(undefined1 *)*(undefined4 *)0x4a7e;
  uVar3 = *(undefined1 *)*(undefined4 *)0x5afa;
  *(undefined1 *)0x4e84 = uVar3;
  *(undefined1 *)0x6988 = *(undefined1 *)*(undefined4 *)0x52da;
  uVar3 = FUN_12ab_05bc(uVar3);
  *(undefined1 *)0x5acc = uVar3;
  if (((*(char *)0x5aaa == '\0') || ((*(byte *)0x6988 & *(byte *)0x5aaa) != 0)) && (in_AX == 0)) {
    bVar2 = false;
  }
  else {
    bVar2 = true;
  }
  bVar1 = *(byte *)0x4e84;
  if (bVar2) {
    FUN_1a47_0552();
    if (*(int *)0x4d0 == 0) {
      if ((*(char *)0x5acc == '\x19') || (*(char *)0x5acc == '\x1a')) {
        local_24 = 1;
      }
      else {
        local_24 = 0;
      }
      uVar10 = 0;
      uVar9 = 1;
LAB_1a47_09ce:
      FUN_1a47_06da(uVar9,local_24,uVar10);
      return;
    }
  }
  else {
    local_6 = '\0';
    if ((*(char *)0x5acc == '\x19') || (*(char *)0x5acc == '\x1a')) {
      local_1e = FUN_1a47_01ae();
      local_6 = '\x01';
    }
    if ((local_6 == '\0') || (local_1e != 0)) {
      if (*(byte *)0x5acc < 0x18) {
        bVar4 = *(byte *)0x5acc & 7;
      }
      else {
        bVar4 = *(byte *)0x5acc;
      }
      FUN_1a47_05b2();
      if (*(int *)0x4d0 == 0) {
        FUN_1a47_06da(0,local_6,0);
      }
      if ((bVar4 != 1) &&
         (((7 < *(byte *)0x5acc && (*(byte *)0x5acc < 0x10)) ||
          ((0xf < *(byte *)0x5acc && (*(byte *)0x5acc < 0x18)))))) {
        FUN_1a47_0418();
        FUN_1a47_0552();
      }
      if ((*(byte *)0x4c6a & 0x40) != 0) {
        FUN_1a47_0552();
      }
      if (((*(byte *)0x4e84 & 0x20) != 0) && (local_6 == '\0')) {
        FUN_1a47_036e();
        FUN_1a47_0552();
      }
      if (((*(byte *)0x4e84 & 0x40) != 0) && (local_6 == '\0')) {
        FUN_1a47_030e();
        FUN_1a47_0552();
      }
      if ((*(int *)0x4d0 == 0) && (*(int *)0x4da == 0)) {
        iVar6 = FUN_12ab_0458(*(undefined2 *)0x5f16,*(undefined2 *)0x634c);
        if ((iVar6 != -1) && (*(int *)0x4d6 == 0)) {
          FUN_1a47_0552();
        }
        iVar6 = FUN_12ab_0540(*(undefined2 *)0x5f16,*(undefined2 *)0x634c);
        if (iVar6 != 0) {
          FUN_1a47_0552();
        }
      }
      if ((((*(byte *)0x4c6a & 10) != 0) && (local_6 == '\0')) && (*(int *)0x4da == 0)) {
        uVar7 = FUN_1a47_04de();
        if (uVar7 == 0) {
          FUN_1a47_0552();
        }
        else {
          local_1c = 1;
          local_18 = 0;
          do {
            if ((local_1c & uVar7) != 0) {
              FUN_1a47_0552();
            }
            local_1c = local_1c << 1;
            local_18 = local_18 + 1;
          } while (local_18 < 8);
        }
      }
      if (local_6 != '\0') {
        local_8 = -1;
        if ((*(byte *)0x5ad3 & 0xdd) == 0xc1) {
          local_8 = 0;
        }
        if ((*(byte *)0x5ad3 & 0x77) == 7) {
          local_8 = 1;
        }
        if ((*(byte *)0x5ad3 & 0x77) == 0x70) {
          local_8 = 2;
        }
        if ((*(byte *)0x5ad3 & 0xdd) == 0x1c) {
          local_8 = 3;
        }
        if (local_8 < 0) {
          local_e = 0;
          do {
            *(char *)0x714 = ((byte)local_e + 1 & 2) << 2;
            *(char *)0x715 = ((byte)local_e & 0xfe) << 2;
            FUN_1a47_0552();
            local_e = local_e + 1;
          } while (local_e < 4);
          *(undefined1 *)0x715 = 0;
          *(undefined1 *)0x714 = 0;
        }
        else {
          *(undefined1 *)0x715 = 0;
          *(undefined1 *)0x714 = 0;
          FUN_1a47_0552();
        }
        FUN_1a47_0676();
        if ((bVar1 & 0xc0) != 0) {
          for (local_e = 0; local_e < 4; local_e = local_e + 1) {
            if (*(char *)(local_e + 0x6ba) < '\0') {
              iVar6 = -*(int *)0x4e70;
            }
            else {
              iVar6 = 0;
            }
            if (*(char *)(local_e + 0x6ba) < '\x01') {
              iVar8 = 0;
            }
            else {
              iVar8 = *(int *)0x4e70;
            }
            bVar1 = *(byte *)((int)*(char *)(local_e + 0x6b4) + *(int *)0x5afa + iVar6 + iVar8);
            if ((((bVar1 & 0x40) != 0) && (cVar5 = FUN_12ab_05bc(bVar1), cVar5 != '\x19')) &&
               (cVar5 != '\x1a')) {
              FUN_1a47_0552();
            }
          }
        }
        if (((*(int *)0x4d0 == 0) && (*(int *)0x4d6 == 0)) &&
           (iVar6 = FUN_12ab_0458(*(undefined2 *)0x5f16,*(undefined2 *)0x634c), iVar6 != -1)) {
          FUN_1a47_0552();
        }
      }
    }
    else {
      FUN_1a47_05b2();
      if (*(int *)0x4d0 == 0) {
        iVar6 = FUN_12ab_0458(*(undefined2 *)0x5f16,*(undefined2 *)0x634c);
        if ((iVar6 != -1) && (*(int *)0x4d6 == 0)) {
          FUN_1a47_0552();
        }
        uVar10 = 1;
        local_24 = 1;
        uVar9 = 0;
        goto LAB_1a47_09ce;
      }
    }
  }
  return;
}



void __stdcall16far FUN_1a47_0d66(int param_1,int param_2)

{
  bool bVar1;
  bool bVar2;
  int in_AX;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int in_DX;
  int iVar8;
  int in_BX;
  undefined2 unaff_DS;
  int local_28;
  int local_26;
  int local_20;
  int local_16;
  int local_14;
  int local_e;
  int local_c;
  int local_8;
  int local_6;
  
  local_20 = 0;
  if (param_1 < 0) {
    *(undefined1 *)0x5aaa = 0;
  }
  else {
    *(char *)0x5aaa = '\x01' << ((char)param_1 + 4U & 0x1f);
  }
  FUN_1a47_0006();
  if ((in_AX <= *(int *)0x6026) && (in_DX <= *(int *)0x603e)) {
    iVar3 = in_BX + in_AX + -1;
    iVar4 = param_2 + in_DX + -1;
    if (*(int *)0x6026 < iVar3) {
      iVar3 = *(int *)0x6026;
    }
    if (in_AX < *(int *)0x49f2) {
      in_AX = *(int *)0x49f2;
    }
    iVar5 = *(int *)0x603e;
    if (iVar4 < *(int *)0x603e) {
      iVar5 = iVar4;
    }
    if (in_DX < *(int *)0x49f4) {
      in_DX = *(int *)0x49f4;
    }
    iVar4 = *(int *)0x49f2;
    iVar8 = in_DX - *(int *)0x49f4;
    if (*(int *)0x4a6 == 0) {
      iVar6 = in_DX;
      if (in_DX < 1) {
        iVar6 = 1;
      }
      iVar7 = in_AX;
      if (in_AX < 1) {
        iVar7 = 1;
      }
      local_c = iVar8;
      local_e = FUN_12ab_00fa(iVar7,iVar6);
      local_6 = local_c;
      local_8 = FUN_12ab_012e(iVar7,iVar6);
      local_26 = local_6;
      local_28 = FUN_12ab_02e4(iVar7,iVar6);
      iVar6 = *(int *)0x4b12;
    }
    else {
      local_28 = (in_AX - iVar4) + (iVar8 + 1) * *(int *)0x4e70 + 1;
      local_e = local_28 + *(int *)0x4a8;
      local_c = *(int *)0x4aa;
      local_8 = local_28 + *(int *)0x4ac;
      local_6 = *(int *)0x4ae;
      local_28 = local_28 + *(int *)0x4b4;
      local_26 = *(int *)0x4b6;
      iVar6 = *(int *)0x4e70;
    }
    *(int *)0x4b16 = (*(int *)0x5af4 + iVar8 + 1) * *(int *)0x4e8c + -1;
    for (local_16 = in_DX; local_16 <= iVar5; local_16 = local_16 + 1) {
      *(int *)0x634c = local_16;
      *(int *)0x5afa = local_e;
      *(int *)0x5afc = local_c;
      *(int *)0x4a7e = local_8;
      *(int *)0x4a80 = local_6;
      *(int *)0x52da = local_28;
      *(int *)0x52dc = local_26;
      if ((local_16 < 1) || (*(int *)0x4b14 + -1 <= local_16)) {
        bVar2 = false;
      }
      else {
        bVar2 = true;
      }
      if ((*(int *)0x4d6 != 0) && (local_20 = local_16 - *(int *)0x4ca, local_20 < 1)) {
        local_20 = ~(local_16 - *(int *)0x4ca) + 1;
      }
      *(undefined2 *)0x4e90 = 0;
      *(int *)0x4b0e = (*(int *)0x4e8a >> 1) + (*(int *)0x5ad8 + (in_AX - iVar4)) * *(int *)0x4e8a;
      for (local_14 = in_AX; local_14 <= iVar3; local_14 = local_14 + 1) {
        *(int *)0x5f16 = local_14;
        if ((local_14 < 1) || (*(int *)0x4b12 + -1 <= local_14)) {
          bVar1 = false;
        }
        else {
          bVar1 = true;
        }
        if (*(int *)0x4d6 != 0) {
          iVar8 = local_14 - *(int *)0x4c8;
          if (iVar8 < 1) {
            iVar8 = ~(local_14 - *(int *)0x4c8) + 1;
          }
          FUN_12ab_0040(iVar8,local_20,*(undefined2 *)0x4d6);
        }
        FUN_1a47_0932();
        *(int *)0x4b0e = *(int *)0x4b0e + *(int *)0x4e8a;
        if ((bVar1) && (bVar2)) {
          *(int *)0x5afa = *(int *)0x5afa + 1;
          *(int *)0x4a7e = *(int *)0x4a7e + 1;
          *(int *)0x52da = *(int *)0x52da + 1;
        }
        *(byte *)0x4e90 = *(byte *)0x4e90 ^ 1;
      }
      *(int *)0x4b16 = *(int *)0x4b16 + *(int *)0x4e8c;
      if (bVar2) {
        local_e = local_e + iVar6;
        local_8 = local_8 + iVar6;
        local_28 = local_28 + iVar6;
      }
    }
  }
  return;
}



void __cdecl16far FUN_1a47_1022(void)

{
  undefined2 *puVar1;
  undefined2 in_AX;
  undefined2 unaff_DS;
  
  FUN_1a47_0006();
  if ((*(int *)0x5ad8 != 0) || (*(int *)0x5af4 != 0)) {
    if (*(int *)0x90 == 0) {
      FUN_1c49_000e(*(undefined2 *)0x3afc,*(undefined2 *)0x3afe,*(undefined2 *)0x3b00,
                    *(undefined2 *)0x3b02);
    }
    else {
      puVar1 = (undefined2 *)*(int *)0x90;
      FUN_1cb9_0000(*(undefined2 *)0x3afc,*(undefined2 *)0x3afe,*(undefined2 *)0x3b00,
                    *(undefined2 *)0x3b02,*puVar1,puVar1[1],puVar1[2],puVar1[3],0,0,
                    *(undefined2 *)0x3afe,*(undefined2 *)0x3afc,0,0xfff8);
    }
  }
  FUN_1a47_0d66(in_AX,*(undefined2 *)0x52d8);
  return;
}



void __cdecl16far FUN_1a47_10b8(void)

{
  undefined2 unaff_DS;
  undefined2 local_6;
  undefined2 local_4;
  
  for (local_6 = 0; local_6 < *(int *)0x4b14; local_6 = local_6 + 1) {
    for (local_4 = 0; local_4 < *(int *)0x4b12; local_4 = local_4 + 1) {
      FUN_12ab_022c(local_4,local_6,0xffff);
    }
  }
  return;
}



undefined1 __cdecl16far FUN_1b56_0004(void)

{
  undefined2 unaff_DS;
  undefined1 local_10a [136];
  undefined1 local_82;
  undefined2 local_a;
  undefined2 local_8;
  undefined1 *local_6;
  
  local_a = 0x10;
  local_8 = 0x10;
  local_6 = local_10a;
  FUN_1d8f_0000(0,*(undefined2 *)0x4c0,*(undefined2 *)0x4c2);
  return local_82;
}



undefined1 __cdecl16far FUN_1b56_0038(undefined2 param_1)

{
  undefined2 unaff_DS;
  undefined1 local_10a [136];
  undefined1 local_82;
  undefined2 local_a;
  undefined2 local_8;
  undefined1 *local_6;
  
  local_a = 0x10;
  local_8 = 0x10;
  local_6 = local_10a;
  FUN_130b_0048(*(undefined2 *)0x4b8,*(undefined2 *)0x4ba,param_1,&local_a,0,0);
  return local_82;
}



void __cdecl16far FUN_1b56_006c(void)

{
  undefined1 uVar1;
  int iVar2;
  undefined2 unaff_DS;
  
  iVar2 = 0;
  do {
    uVar1 = FUN_1b56_0038(iVar2);
    *(undefined1 *)(iVar2 + 0x4aec) = uVar1;
    uVar1 = FUN_1b56_0038(iVar2);
    *(undefined1 *)(iVar2 + 0x4afc) = uVar1;
    uVar1 = FUN_1b56_0038(iVar2);
    *(undefined1 *)(iVar2 + 0x4af4) = uVar1;
    iVar2 = iVar2 + 1;
  } while (iVar2 < 8);
  uVar1 = FUN_1b56_0038(0x18);
  *(undefined1 *)0x4b04 = uVar1;
  uVar1 = FUN_1b56_0038(0x19);
  *(undefined1 *)0x4b05 = uVar1;
  uVar1 = FUN_1b56_0038(0x1a);
  *(undefined1 *)0x4b06 = uVar1;
  uVar1 = FUN_1b56_0004(0x21);
  *(undefined1 *)0x4b07 = uVar1;
  uVar1 = FUN_1b56_0004(0x31);
  *(undefined1 *)0x4b08 = uVar1;
  return;
}



void __cdecl16far FUN_1b56_00dc(void)

{
  int iVar1;
  int iVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  
  *(int *)0x49c2 = *(int *)0x4ca + -0x13;
  iVar1 = *(int *)0x4b12;
  iVar2 = *(int *)0x4c8;
  *(int *)0x6982 = iVar2 + -0x1c;
  uVar3 = FUN_1865_000e(iVar2 + -0x1c,1,iVar1 + -0x39);
  *(undefined2 *)0x6982 = uVar3;
  uVar3 = FUN_1865_000e(*(undefined2 *)0x49c2,1,*(int *)0x4b14 + -0x28);
  *(undefined2 *)0x49c2 = uVar3;
  return;
}



void __cdecl16far
FUN_1b56_011c(undefined2 param_1,int param_2,int param_3,int param_4,int param_5,int param_6)

{
  int iVar1;
  int iVar2;
  undefined1 *puVar3;
  int iVar4;
  int iVar5;
  byte *pbVar6;
  byte *pbVar7;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 *local_32;
  byte *local_2e;
  byte *local_2a;
  int local_24;
  uint local_1e;
  byte local_1b;
  int local_6;
  undefined1 local_3;
  
  if (param_5 < 0) {
    local_1e = 0;
  }
  else {
    local_1e = 0x10 << ((byte)param_5 & 0x1f);
  }
  local_2a = (byte *)FUN_12ab_00fa(param_1,param_2);
  iVar4 = param_5;
  FUN_12ab_012e(param_1,param_2);
  local_2e = (byte *)FUN_12ab_02e4(param_1,param_2);
  FUN_12ab_0198(param_1,param_2);
  iVar5 = (param_2 - *(int *)0x49c2) + 9;
  local_32 = (undefined1 *)FUN_1c91_0000();
  iVar1 = *(int *)0x4b12;
  iVar2 = *(int *)0x3af6;
  if (0 < param_4) {
    local_24 = param_4;
    do {
      if (0 < param_3) {
        local_6 = param_3;
        puVar3 = local_32;
        pbVar6 = local_2a;
        pbVar7 = local_2e;
        do {
          local_2a = pbVar6 + 1;
          local_1b = *pbVar6;
          local_2e = pbVar7 + 1;
          if (param_6 == 0) {
            if ((local_1e == 0) || ((local_1e & *pbVar7) != 0)) {
              if ((local_1b & 0x20) == 0) {
                local_1b = local_1b & 0x1f;
              }
              else {
                local_1b = (-((local_1b & 0x80) == 0) & 1U) + 0x1b;
              }
              local_3 = *(undefined1 *)(local_1b + 0x4aec);
            }
            else {
              local_3 = 0;
            }
          }
          else {
            local_3 = 0xf;
          }
          local_32 = puVar3 + 1;
          *puVar3 = local_3;
          local_6 = local_6 + -1;
          puVar3 = local_32;
          pbVar6 = local_2a;
          pbVar7 = local_2e;
        } while (local_6 != 0);
      }
      local_2a = local_2a + (iVar1 - param_3);
      local_2e = local_2e + (iVar1 - param_3);
      local_32 = local_32 + (iVar2 - param_3);
      local_24 = local_24 + -1;
    } while (local_24 != 0);
  }
  return;
}



void __cdecl16far FUN_1b56_0274(undefined2 param_1)

{
  undefined2 unaff_DS;
  
  FUN_1b56_011c(*(undefined2 *)0x6982,*(undefined2 *)0x49c2,0x38,0x27,param_1,0);
  return;
}



void __cdecl16far
FUN_1b56_028e(int param_1,int param_2,int param_3,int param_4,int param_5,undefined2 param_6,
             undefined2 param_7)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  undefined2 unaff_DS;
  
  FUN_1b56_00dc();
  iVar4 = param_2;
  if (param_2 < *(int *)0x49c2) {
    iVar4 = *(int *)0x49c2;
  }
  iVar1 = param_2 + param_4 + -1;
  if (*(int *)0x49c2 + 0x26 < iVar1) {
    iVar1 = *(int *)0x49c2 + 0x26;
  }
  iVar1 = (iVar1 - iVar4) + 1;
  if (iVar1 < 0) {
    iVar1 = 0;
  }
  param_3 = param_1 + param_3;
  if (param_1 < *(int *)0x6982) {
    param_1 = *(int *)0x6982;
  }
  param_3 = param_3 + -1;
  if (*(int *)0x6982 + 0x37 < param_3) {
    param_3 = *(int *)0x6982 + 0x37;
  }
  iVar3 = (param_3 - param_1) + 1;
  if (iVar3 < 0) {
    iVar3 = 0;
  }
  if ((iVar3 != 0) && (iVar1 != 0)) {
    FUN_1b56_011c(param_1,iVar4,iVar3,iVar1,param_6,param_7);
    iVar2 = *(int *)0x49c2 + 0x26;
    if (*(int *)0x603e < iVar2) {
      iVar2 = *(int *)0x603e;
    }
    FUN_1c86_000c(0xf,(iVar2 - *(int *)0x49c2) + 9,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                  *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
    if (param_5 != 0) {
      FUN_1c34_0044(iVar1,iVar3,(iVar4 - *(int *)0x49c2) + 9);
    }
  }
  return;
}



void __cdecl16far FUN_1b56_03b4(int param_1,undefined2 param_2)

{
  undefined2 *puVar1;
  int iVar2;
  undefined2 unaff_DS;
  
  FUN_1b56_00dc();
  if (*(int *)0x90 == 0) {
    FUN_1c5b_0004(0,0x29,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                  *(undefined2 *)0x3afa);
  }
  else {
    puVar1 = (undefined2 *)*(int *)0x90;
    FUN_1cb9_0000(*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                  *(undefined2 *)0x3afa,*puVar1,puVar1[1],puVar1[2],puVar1[3],0xf1,8,0x4f,0x29,0,0);
  }
  FUN_1c86_000c(6,0x30,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,*(undefined2 *)0x3af8,
                *(undefined2 *)0x3afa);
  FUN_1b56_0274(param_2);
  iVar2 = *(int *)0x49c2 + 0x26;
  if (*(int *)0x603e < iVar2) {
    iVar2 = *(int *)0x603e;
  }
  FUN_1c86_000c(0xf,(iVar2 - *(int *)0x49c2) + 9,*(undefined2 *)0x3af4,*(undefined2 *)0x3af6,
                *(undefined2 *)0x3af8,*(undefined2 *)0x3afa);
  if (param_1 != 0) {
    FUN_1c34_0044(0x29,0x4f,8);
  }
  return;
}



void __cdecl16far FUN_1b56_04ca(void)

{
  FUN_1b56_00dc();
  return;
}



undefined2 __stdcall16far
FUN_1ba3_0000(undefined2 param_1,int param_2,int param_3,undefined2 param_4)

{
  undefined2 in_AX;
  undefined2 in_DX;
  undefined2 in_BX;
  int iVar1;
  int iVar2;
  undefined2 unaff_DS;
  
  if (*(int *)0x716 == 0x78) {
    FUN_1ed0_03d6(param_2,param_2 >> 0xf,param_3,param_3 >> 0xf);
    return 0;
  }
  iVar1 = *(int *)0x716 + 1;
  iVar2 = iVar1 * 0x10;
  *(undefined2 *)(iVar2 + 0x530a) = in_AX;
  *(undefined2 *)(iVar2 + 0x530c) = in_DX;
  *(undefined2 *)(iVar2 + 0x530e) = in_BX;
  *(undefined2 *)(iVar2 + 0x5310) = param_4;
  *(int *)(iVar2 + 0x5312) = param_3;
  *(int *)(iVar2 + 0x5314) = param_2;
  *(undefined2 *)(iVar2 + 0x5318) = 0xffff;
  *(undefined2 *)(iVar2 + 0x5316) = param_1;
  *(int *)0x716 = iVar1;
  return 0xffff;
}



uint __cdecl16far FUN_1baa_0002(void)

{
  uint in_AX;
  uint uVar1;
  uint in_DX;
  uint in_BX;
  uint *puVar2;
  undefined2 unaff_DS;
  
  uVar1 = 1;
  puVar2 = (uint *)0x531a;
  while( true ) {
    if (*(uint *)0x716 < uVar1) {
      return 0;
    }
    if ((((puVar2[7] != 0) && (*puVar2 <= in_AX)) && (in_AX <= puVar2[2])) &&
       (((puVar2[1] <= in_DX && (in_DX <= puVar2[3])) && (in_BX == puVar2[6])))) break;
    uVar1 = uVar1 + 1;
    puVar2 = puVar2 + 8;
  }
  return uVar1;
}



undefined2 __cdecl16far FUN_1baf_0004(void)

{
  code *pcVar1;
  undefined1 in_ZF;
  
  pcVar1 = (code *)swi(0x16);
  (*pcVar1)();
  if ((bool)in_ZF) {
    return 0;
  }
  return 1;
}



uint __cdecl16far FUN_1baf_0018(void)

{
  code *pcVar1;
  byte bVar2;
  undefined1 extraout_AH;
  
  pcVar1 = (code *)swi(0x16);
  bVar2 = (*pcVar1)();
  if (bVar2 != 0) {
    return (uint)bVar2;
  }
  return CONCAT11(1,extraout_AH);
}



// WARNING: Control flow encountered bad instruction data
// WARNING: Instruction at (ram,0x00010108) overlaps instruction at (ram,0x00010107)
// 

uint * __cdecl16far FUN_1bb2_000e(uint *param_1,undefined2 param_2,char *param_3,undefined2 param_4)

{
  byte *pbVar1;
  int *piVar2;
  uint *puVar3;
  byte *pbVar4;
  int *piVar5;
  int iVar6;
  code *pcVar7;
  uint uVar8;
  uint uVar9;
  int iVar10;
  uint *puVar11;
  undefined1 *puVar12;
  int iVar13;
  int iVar14;
  int iVar15;
  int iVar16;
  int iVar17;
  uint *in_AX;
  uint *puVar18;
  undefined2 in_CX;
  byte bVar19;
  uint *in_BX;
  uint *unaff_SI;
  uint *unaff_DI;
  undefined2 uVar20;
  int unaff_ES;
  undefined2 uVar21;
  int unaff_SS;
  int unaff_DS;
  bool bVar22;
  undefined4 uVar23;
  uint *puVar24;
  char *in_stack_00000002;
  byte abStack_1ac [62];
  int iStack_16e;
  int iStack_16c;
  int iStack_16a;
  uint *puStack_168;
  int iStack_166;
  undefined4 uStack_164;
  char acStack_160 [80];
  int iStack_110;
  undefined4 uStack_10e;
  int iStack_10a;
  char cStack_108;
  char cStack_3b;
  int iStack_2e;
  int iStack_2a;
  int iStack_24;
  int iStack_20;
  int iStack_1e;
  int iStack_1a;
  int iStack_16;
  undefined4 uStack_14;
  char *pcStack_10;
  uint *puStack_e;
  uint *puStack_c;
  uint *in_stack_0000fff6;
  char *pcVar25;
  int in_stack_0000fff8;
  char *pcVar26;
  
  puVar11 = in_AX + -0x88;
  if ((uint *)0x22 < puVar11) {
    if ((0x60 < (int)in_AX) && ((int)in_AX < 0x7b)) {
      in_AX = in_AX + -0x10;
    }
    return in_AX;
  }
  puVar18 = (uint *)((int)puVar11 * 2);
  bVar22 = puVar18 == (uint *)0x0;
  uVar21 = 0x1000;
  bVar19 = (byte)((uint)in_AX >> 8);
  switch(puVar11) {
  case (uint *)0x1:
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x92b6,(uint)uStack_14);
    FUN_16d7_07de();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x16d7;
    uStack_14 = (int *)CONCAT22(0x92ce,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x92e9,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(int *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9304,(uint)uStack_14);
    FUN_16d7_07de();
    iVar10 = FUN_1842_001a();
    if (iVar10 != 0) goto LAB_18f9_0b03;
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9332,(uint)uStack_14);
    FUN_16d7_0642();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x934e,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x936a,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9386,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(int *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x93a2,(uint)uStack_14);
    FUN_16d7_07de();
    break;
  default:
    *(char *)((int)unaff_DI + 9) = *(char *)((int)unaff_DI + 9) << 1;
    *(undefined2 *)0x4a4 = 5;
    if (in_stack_0000fff8 != 0) {
      FUN_2388_02c2();
    }
    return in_AX;
  case (uint *)0x3:
    goto switchD_1000_bb3f_caseD_3;
  case (uint *)0x4:
    break;
  case (uint *)0x6:
    *(char *)0x6a50 = (*(char *)0x6a50 - (char)puVar18) - ((int)puVar11 < 0);
  case (uint *)0x0:
switchD_1000_bb3f_caseD_3:
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x560,(uint)uStack_14);
    FUN_16d7_07de();
    return (uint *)0x0;
  case (uint *)0x7:
    goto switchD_1000_bb3f_caseD_7;
  case (uint *)0x9:
    *(int *)(&stack0x0016 + (int)unaff_SI) = *(int *)(&stack0x0016 + (int)unaff_SI) + (int)in_BX;
    goto switchD_1000_bb3f_caseD_3;
  case (uint *)0xa:
    goto code_r0x000197a3;
  case (uint *)0xc:
    goto LAB_1000_04a9;
  case (uint *)0xd:
    if (((int)puVar11 < 0 || bVar22) || ((char)*unaff_SI == '^')) {
      if (param_3[1] == '^') {
        *(uint *)CONCAT22(in_stack_0000fff8,in_stack_0000fff6) = 1;
        param_3 = param_3 + 2;
      }
      else {
        *(byte *)CONCAT22(in_stack_0000fff8,in_stack_0000fff6) =
             (byte)*(uint *)CONCAT22(in_stack_0000fff8,in_stack_0000fff6) | 2;
        param_3 = param_3 + 1;
      }
    }
    puVar11 = param_1 + 0x42;
    puStack_c = (uint *)param_3;
    puStack_e = (uint *)0x1000;
    pcStack_10 = (char *)0x40e9;
    FUN_2388_0dd4();
    puStack_c = (uint *)0x40f4;
    uVar23 = FUN_1f45_010a();
    puVar18 = (uint *)((ulong)uVar23 >> 0x10);
    *(undefined2 *)0x238a = (int)uVar23;
    *(undefined2 *)0x238c = puVar18;
    puStack_c = (uint *)*(undefined2 *)0x238a;
    puStack_e = (uint *)0x1f45;
    pcStack_10 = (char *)0x410f;
    FUN_2388_0dec();
    param_1[2] = param_1[2] + 1;
    return puVar18;
  case (uint *)0xe:
  case (uint *)0x11:
  case (uint *)0x14:
    pcVar7 = (code *)swi(3);
    puVar11 = (uint *)(*pcVar7)();
    return puVar11;
  case (uint *)0xf:
    *(char *)((int)unaff_SI + 5) = *(char *)((int)unaff_SI + 5) + bVar19;
    do {
      in_AX = (uint *)0x0;
      do {
        FUN_1c21_0110();
code_r0x000100ab:
        if (in_AX == (uint *)0x0) {
          return in_stack_0000fff6;
        }
        FUN_1c21_0042();
        iVar10 = FUN_1baf_0004();
        if (iVar10 != 0) {
          in_stack_0000fff6 = (uint *)FUN_1baf_0018();
          in_AX = (uint *)0x0;
        }
        bVar22 = *(int *)0x730 == 0;
switchD_1000_bb3f_caseD_12:
      } while (bVar22);
    } while( true );
  case (uint *)0x10:
    uVar8 = *(uint *)(param_3 + (int)puVar11 * 0x18 + 0x3e);
    puVar18 = *(uint **)(param_3 + (int)puVar11 * 0x18 + 0x40);
    if ((int)*(uint **)(param_3 + (int)puVar11 * 0x18 + 0x40) < (int)puStack_e) {
      puVar18 = puStack_e;
    }
    puStack_c[1] = (int)puVar18 + *(uint *)CONCAT22(in_stack_0000fff6,puStack_c) + param_1[0x23];
    uVar9 = param_1[0x17];
    if ((int)param_1[0x17] < (int)uVar8) {
      uVar9 = uVar8;
    }
    param_1[0x17] = uVar9;
    uVar8 = param_1[0x18];
    if ((int)param_1[0x18] < (int)(uint)uStack_14) {
      uVar8 = (uint)uStack_14;
    }
    param_1[0x18] = uVar8;
    param_1[3] = param_1[3] + 1;
    return puStack_c;
  case (uint *)0x12:
    goto switchD_1000_bb3f_caseD_12;
  case (uint *)0x13:
    do {
      iVar10 = FUN_133d_0f24();
      puStack_168 = (uint *)((int)puStack_168 + iVar10 + 1);
      pcVar26 = (char *)CONCAT22(*(undefined2 *)(in_stack_00000002 + 0x82),
                                 *(char **)(in_stack_00000002 + 0x80));
      uVar23 = FUN_133d_0f24();
      iVar10 = (int)((ulong)uVar23 >> 0x10);
      iStack_110 = iStack_110 + (int)uVar23 + 1;
      do {
        while (iVar17 = FUN_2388_0dd4(), iVar17 != 0) {
          while( true ) {
            if (*pcVar26 != ' ') break;
            pcVar26 = (char *)CONCAT22((int)((ulong)pcVar26 >> 0x10),(char *)pcVar26 + 1);
          }
          puStack_c = (uint *)0x46e3;
          puVar12 = (undefined1 *)FUN_2388_0ca8();
          uStack_164 = (undefined1 *)CONCAT22(iVar10,puVar12);
          if (iVar10 != 0 || puVar12 != (undefined1 *)0x0) {
            *uStack_164 = 0;
          }
          iStack_16c = FUN_2388_0dd4();
          acStack_160[0] = '\0';
          if (cStack_108 != '\0') {
            FUN_2388_05e6();
          }
          puStack_c = (uint *)0x2388;
          puStack_e = (uint *)0x4739;
          FUN_2388_0e22();
          puStack_c = (uint *)0x4754;
          uVar23 = FUN_133d_0494();
          iStack_166 = (int)uVar23;
          if (iStack_10a < *(int *)(in_stack_00000002 + 0x74) + iStack_166 + unaff_SS) {
            if (iStack_16e != 0) {
              puStack_c = (uint *)0x4786;
              FUN_133d_117a();
            }
            iVar10 = FUN_133d_0f24();
            puStack_168 = (uint *)((int)puStack_168 + iVar10 + 1);
            uVar23 = FUN_133d_0f24();
            uVar21 = (undefined2)((ulong)uVar23 >> 0x10);
            iStack_110 = iStack_110 + (int)uVar23 + 1;
            while (uVar23 = CONCAT22(uVar21,iStack_166), acStack_160[0] == ' ') {
              FUN_2388_0626();
            }
            cStack_108 = '\0';
          }
          iVar10 = (int)((ulong)uVar23 >> 0x10);
          iStack_166 = (int)uVar23;
          puStack_c = (uint *)0x2388;
          puStack_e = (uint *)0x47ec;
          FUN_2388_0e22();
          if (uStack_164._2_2_ != 0 || (undefined1 *)uStack_164 != (undefined1 *)0x0) {
            *uStack_164 = 0x20;
          }
          pcVar26 = (char *)CONCAT22(unaff_SS,acStack_160 + iStack_16c);
        }
        uVar21 = (undefined2)((ulong)uStack_10e >> 0x10);
        pbVar4 = *(byte **)((byte *)uStack_10e + 6);
        iVar17 = *(int *)((byte *)uStack_10e + 8);
        uStack_10e = (byte *)CONCAT22(iVar17,pbVar4);
        if (iVar17 == 0 && pbVar4 == (byte *)0x0) {
          if (cStack_108 != '\0') {
            if (iStack_16e != 0) {
              puStack_c = (uint *)0x4865;
              FUN_133d_117a();
            }
            iVar10 = FUN_133d_0f24();
            puStack_168 = (uint *)((int)puStack_168 + iVar10 + 1);
            FUN_133d_0f24();
          }
          return puStack_168;
        }
        iVar10 = *(int *)(pbVar4 + 4);
        pcVar26 = (char *)CONCAT22(iVar10,*(char **)(pbVar4 + 2));
      } while ((*uStack_10e & 3) == 0);
      if (cStack_108 != '\0') {
        if (iStack_16e != 0) {
          puStack_c = (uint *)0x45ff;
          FUN_133d_117a();
        }
        iVar10 = FUN_133d_0f24();
        puStack_168 = (uint *)((int)puStack_168 + iVar10 + 1);
        pcVar26 = (char *)CONCAT22(*(undefined2 *)(in_stack_00000002 + 0x82),
                                   *(char **)(in_stack_00000002 + 0x80));
        iVar10 = FUN_133d_0f24();
        iStack_110 = iStack_110 + iVar10 + 1;
        cStack_108 = '\0';
      }
      if (iStack_16e != 0) {
        if ((*uStack_10e & 1) == 0) {
          iStack_16a = 0;
        }
        else {
          puStack_c = (uint *)0x465e;
          iVar10 = FUN_133d_0494();
          iStack_16a = (iStack_10a >> 1) - (iVar10 >> 1);
        }
        pcVar26 = (char *)CONCAT22(param_1,in_stack_00000002);
        puStack_c = (uint *)0x468b;
        FUN_133d_117a();
      }
      for (; *pcVar26 != '\0';
          pcVar26 = (char *)CONCAT22((int)((ulong)pcVar26 >> 0x10),(char *)pcVar26 + 1)) {
      }
    } while( true );
  case (uint *)0x15:
    *(char *)(uint *)((int)puVar18 + (int)unaff_SI) =
         (char)*(uint *)((int)puVar18 + (int)unaff_SI) + '\x01';
    (&stack0xfffe)[(int)unaff_DI] = (&stack0xfffe)[(int)unaff_DI] + (char)((uint)in_CX >> 8);
    pbVar1 = abStack_1ac + (int)unaff_DI;
    *pbVar1 = *pbVar1 >> 2 | *pbVar1 << 6;
    *(char *)(uint *)((int)puVar18 + (int)unaff_DI) =
         (char)*(uint *)((int)puVar18 + (int)unaff_DI) + (char)in_BX + -0x39 +
         ((char)*pbVar1 < '\0');
    *unaff_SI = *unaff_SI & (uint)puVar18;
    goto code_r0x000100ab;
  case (uint *)0x16:
    cStack_3b = cStack_3b + (char)in_AX;
    piVar2 = (int *)((int)puVar18 + (int)unaff_SI + 1);
    *piVar2 = *piVar2 + CONCAT11((char)((uint)in_BX >> 8),(char)in_BX + (char)in_CX);
    iStack_2e = 0;
    iStack_16 = 0;
    iStack_20 = 0;
    if ((*(int *)(in_stack_00000002 + 8) != 0) && (*(int *)(in_stack_00000002 + 2) != 0)) {
      (in_stack_00000002 + 8)[0] = '\0';
      (in_stack_00000002 + 8)[1] = '\0';
      (in_stack_00000002 + 0x62)[0] = '\0';
      (in_stack_00000002 + 0x62)[1] = '\0';
      (in_stack_00000002 + 0x60)[0] = '\0';
      (in_stack_00000002 + 0x60)[1] = '\0';
    }
    *(undefined2 *)(in_stack_00000002 + 0x10) = *(undefined2 *)(in_stack_00000002 + 0xc);
    *(undefined2 *)(in_stack_00000002 + 0x12) = *(undefined2 *)(in_stack_00000002 + 0xe);
    (in_stack_00000002 + 0x14)[0] = '\0';
    (in_stack_00000002 + 0x14)[1] = '\0';
    *(int *)(in_stack_00000002 + 0x16) =
         *(int *)(in_stack_00000002 + 0x4a) * 2 + *(int *)(in_stack_00000002 + 0x46);
    uVar8 = -(uint)((in_stack_00000002[10] & 0x10U) == 0) & 3;
    *(uint *)(in_stack_00000002 + 0x2a) = uVar8;
    iVar10 = *(int *)(in_stack_00000002 + 0x46);
    *(uint *)(in_stack_00000002 + 0x2c) = uVar8 + iVar10;
    *(uint *)(in_stack_00000002 + 0x24) = uVar8;
    *(uint *)(in_stack_00000002 + 0x26) = uVar8 + iVar10;
    iVar10 = *(int *)(in_stack_00000002 + 0x28);
    if (iVar10 < *(int *)(in_stack_00000002 + 0x20)) {
      iVar10 = *(int *)(in_stack_00000002 + 0x20);
    }
    if (iVar10 < *(int *)(in_stack_00000002 + 0x34)) {
      iVar10 = *(int *)(in_stack_00000002 + 0x34);
    }
    *(int *)(in_stack_00000002 + 0x28) = iVar10;
    *(int *)(in_stack_00000002 + 0x34) = iVar10;
    *(int *)(in_stack_00000002 + 0x20) = iVar10;
    if (*(int *)(in_stack_00000002 + 2) + *(int *)(in_stack_00000002 + 4) +
        *(int *)(in_stack_00000002 + 6) + *(int *)(in_stack_00000002 + 8) != 0) {
      pcVar25 = (char *)0x0;
      if (*(int *)(in_stack_00000002 + 0x5e) != 0 || *(int *)(in_stack_00000002 + 0x5c) != 0) {
        iVar10 = *(int *)(in_stack_00000002 + 0x48) + *(int *)(in_stack_00000002 + 0x2e) +
                 *(int *)(in_stack_00000002 + 0x30) + *(int *)(in_stack_00000002 + 0x32);
        *(int *)(in_stack_00000002 + 0x14) = *(int *)(in_stack_00000002 + 0x14) + iVar10;
        *(int *)(in_stack_00000002 + 0x2a) = *(int *)(in_stack_00000002 + 0x2a) + iVar10;
        *(int *)(in_stack_00000002 + 0x24) = *(int *)(in_stack_00000002 + 0x24) + iVar10;
        *(int *)(in_stack_00000002 + 0x36) = *(int *)(in_stack_00000002 + 0x36) + iVar10;
        iVar10 = *(int *)(in_stack_00000002 + 0x5e);
        iVar17 = *(int *)(in_stack_00000002 + 0x5c);
        while (uStack_14 = (int *)CONCAT22(iVar10,iVar17), iVar10 != 0 || iVar17 != 0) {
          pcVar25 = (char *)*(undefined2 *)(iVar17 + 2);
          piVar2 = (int *)(iVar17 + 0x10);
          iVar10 = *(int *)(iVar17 + 0x12);
          iVar17 = *piVar2;
        }
      }
      if (*(int *)(in_stack_00000002 + 0x5a) != 0 || *(int *)(in_stack_00000002 + 0x58) != 0) {
        iStack_2e = FUN_133d_11a6();
        iVar10 = *(int *)(in_stack_00000002 + 0x46);
        *(int *)(in_stack_00000002 + 0x26) = *(int *)(in_stack_00000002 + 0x26) + iVar10 + iStack_2e
        ;
        *(int *)(in_stack_00000002 + 0x38) = *(int *)(in_stack_00000002 + 0x38) + iVar10 + iStack_2e
        ;
        pcVar25 = in_stack_00000002;
      }
      if (*(int *)(in_stack_00000002 + 0x56) != 0 || *(int *)(in_stack_00000002 + 0x54) != 0) {
        pcVar25 = *(char **)(in_stack_00000002 + 0x80);
        iVar10 = FUN_133d_0f24();
        iStack_16 = (iVar10 + *(int *)(in_stack_00000002 + 0x46)) * *(int *)(in_stack_00000002 + 2);
      }
      if (*(int *)(in_stack_00000002 + 0x62) != 0 || *(int *)(in_stack_00000002 + 0x60) != 0) {
        pcVar25 = *(char **)(in_stack_00000002 + 0x80);
        iVar10 = FUN_133d_0f24();
        iStack_20 = (iVar10 + *(int *)(in_stack_00000002 + 0x46) + 5) *
                    *(int *)(in_stack_00000002 + 8);
      }
      pcStack_10 = (char *)(iStack_20 + iStack_16 + iStack_2e);
      if (pcStack_10 != (char *)0x0) {
        pcStack_10 = pcStack_10 + *(int *)(in_stack_00000002 + 0x46);
      }
      iVar10 = (-(uint)((in_stack_00000002[10] & 0x10U) == 0) & 3) * 2;
      if ((int)pcVar25 < (int)pcStack_10) {
        pcVar25 = pcStack_10;
      }
      *(char **)(in_stack_00000002 + 0x16) = pcVar25 + *(int *)(in_stack_00000002 + 0x16) + iVar10;
      *(int *)(in_stack_00000002 + 0x14) =
           *(int *)(in_stack_00000002 + 0x14) + iVar10 + *(int *)(in_stack_00000002 + 0x20);
      if (*(int *)0x566 != 0) {
        if ((*(int *)(in_stack_00000002 + 0x80) == *(int *)0x80) &&
           (*(int *)(in_stack_00000002 + 0x82) == *(int *)0x82)) {
          *(int *)(in_stack_00000002 + 0x16) = *(int *)(in_stack_00000002 + 0x16) + 6;
        }
        else {
          *(int *)(in_stack_00000002 + 0x16) = *(int *)(in_stack_00000002 + 0x16) + 3;
        }
      }
      if (*(int *)(in_stack_00000002 + 0x10) == -1) {
        *(int *)(in_stack_00000002 + 0x10) = -((*(int *)(in_stack_00000002 + 0x14) >> 1) + -0xa0);
      }
      if (*(int *)(in_stack_00000002 + 0x12) == -1) {
        *(int *)(in_stack_00000002 + 0x12) = -((*(int *)(in_stack_00000002 + 0x16) >> 1) + -100);
      }
      iStack_24 = *(int *)(in_stack_00000002 + 0x16) + *(int *)(in_stack_00000002 + 0x12);
      iStack_1e = *(int *)(in_stack_00000002 + 0x14) + *(int *)(in_stack_00000002 + 0x10);
      if (0x140 < iStack_1e) {
        *(int *)(in_stack_00000002 + 0x10) =
             *(int *)(in_stack_00000002 + 0x10) - (iStack_1e + -0x140);
      }
      if (200 < iStack_24) {
        *(int *)(in_stack_00000002 + 0x12) = *(int *)(in_stack_00000002 + 0x12) + (200 - iStack_24);
      }
      if ((*(int *)(in_stack_00000002 + 0x10) < 0) || (*(int *)(in_stack_00000002 + 0x12) < 0)) {
        puStack_c = *(uint **)(in_stack_00000002 + 0x12);
        puStack_e = (uint *)0x1000;
        pcStack_10 = (char *)0x4b40;
        FUN_1ed0_03d6();
      }
      *(undefined2 *)(in_stack_00000002 + 0x18) = *(undefined2 *)(in_stack_00000002 + 0x10);
      *(undefined2 *)(in_stack_00000002 + 0x1a) = *(undefined2 *)(in_stack_00000002 + 0x12);
      iVar10 = *(int *)(in_stack_00000002 + 0x14);
      *(int *)(in_stack_00000002 + 0x1c) = iVar10;
      *(undefined2 *)(in_stack_00000002 + 0x1e) = *(undefined2 *)(in_stack_00000002 + 0x16);
      if (*(int *)(in_stack_00000002 + 0x6a) != 0 || *(int *)(in_stack_00000002 + 0x68) != 0) {
        piVar5 = *(int **)(in_stack_00000002 + 0x68);
        uVar21 = *(undefined2 *)(in_stack_00000002 + 0x6a);
        uStack_14 = (int *)CONCAT22(uVar21,piVar5);
        if (-1 < *(int *)0x55c) {
          uVar20 = (undefined2)((ulong)*(undefined4 *)(piVar5 + 6) >> 0x10);
          iVar16 = (int)*(undefined4 *)(piVar5 + 6);
          iVar17 = *(int *)(iVar16 + 0x4c) + 3;
          iStack_1a = -3;
          iVar16 = *(int *)(iVar16 + 0x4a) + 3;
          iVar10 = iVar10 + iVar16;
          iStack_2a = iVar10 + 3;
          if (0x140 < iStack_2a) {
            iStack_1a = iVar10 + -0x140;
            iStack_2a = 0x140;
          }
          if ((((*(int *)0x55c == 0) || (*(int *)0x55c == 3)) || (*(int *)0x55c == 5)) ||
             ((*(int *)0x55c == 7 || (*(int *)0x55c == 8)))) {
            bVar22 = true;
          }
          else {
            bVar22 = false;
          }
          *(int *)(in_stack_00000002 + 0x1c) = iStack_2a;
          if (bVar22) {
            iVar10 = -((iStack_2a >> 1) + -0xa0);
            *(int *)(in_stack_00000002 + 0x18) = iVar10;
            piVar5[2] = iVar10;
            *(int *)(in_stack_00000002 + 0x10) = (iVar10 - iStack_1a) + iVar16;
          }
          else {
            iVar10 = -((iStack_2a >> 1) + -0xa0);
            *(int *)(in_stack_00000002 + 0x18) = iVar10;
            *(int *)(in_stack_00000002 + 0x10) = iVar10;
            piVar5[2] = (iVar10 + *(int *)(in_stack_00000002 + 0x14)) - iStack_1a;
          }
          iVar16 = (iVar17 >> 1) + -100;
          iVar13 = -iVar16;
          *uStack_14 = iVar13;
          iVar10 = iVar13;
          if (-*(int *)(in_stack_00000002 + 0x12) != iVar16 &&
              *(int *)(in_stack_00000002 + 0x12) <= iVar13) {
            iVar10 = *(int *)(in_stack_00000002 + 0x12);
          }
          *(int *)(in_stack_00000002 + 0x1a) = iVar10;
          iVar17 = iVar13 + iVar17 + -1;
          if (iVar17 < iStack_24) {
            iVar17 = iStack_24;
          }
          *(int *)(in_stack_00000002 + 0x1e) = (iVar17 - iVar10) + 1;
        }
        if ((-1 < *(int *)0x55e) || (-1 < *(int *)0x560)) {
          uVar20 = (undefined2)((ulong)*(undefined4 *)(piVar5 + 6) >> 0x10);
          iVar10 = (int)*(undefined4 *)(piVar5 + 6);
          iVar17 = *(int *)(iVar10 + 0x4a);
          iVar16 = *(int *)(iVar10 + 0x4c);
          iVar13 = *(int *)(iVar10 + 0x10);
          iVar6 = *(int *)(iVar10 + 0x12);
          iVar10 = *(int *)(iVar10 + 0x14);
          iVar14 = iVar13;
          if (iVar16 < iVar13) {
            iVar14 = iVar16;
          }
          iVar14 = (iVar16 - iVar14) + *(int *)(in_stack_00000002 + 0x16);
          if (iVar14 < 200) {
            iVar15 = -((iVar14 >> 1) + -100);
            *uStack_14 = iVar15;
            *(int *)(in_stack_00000002 + 0x1a) = iVar15;
            if (iVar14 < iVar16) {
              iVar14 = iVar16;
            }
            *(int *)(in_stack_00000002 + 0x1e) = iVar14;
            *(int *)(in_stack_00000002 + 0x12) = (iVar15 - iVar13) + iVar16;
            if (iVar6 == 0) {
              iStack_2a = (*(int *)(in_stack_00000002 + 0x14) - iVar10) + iVar17;
              if (0x140 < iStack_2a) {
                iVar10 = iVar10 + iStack_2a + -0x140;
                iStack_2a = 0x140;
              }
              iVar16 = -((iStack_2a >> 1) + -0xa0);
              piVar5[2] = iVar16;
              *(int *)(in_stack_00000002 + 0x18) = iVar16;
              *(int *)(in_stack_00000002 + 0x1c) = iStack_2a;
              *(int *)(in_stack_00000002 + 0x10) = (iVar16 - iVar10) + iVar17;
            }
            else if (iVar6 == 1) {
              iVar16 = -((iVar17 >> 1) + -0xa0);
              piVar5[2] = iVar16;
              iVar10 = *(int *)(in_stack_00000002 + 0x10);
              if (iVar16 < *(int *)(in_stack_00000002 + 0x10)) {
                iVar10 = iVar16;
              }
              *(int *)(in_stack_00000002 + 0x18) = iVar10;
              iVar17 = iVar16 + iVar17 + -1;
              if (iVar17 < iStack_1e) {
                iVar17 = iStack_1e;
              }
              *(int *)(in_stack_00000002 + 0x1c) = (iVar17 - iVar10) + 1;
            }
            else if (iVar6 == 2) {
              iStack_2a = (*(int *)(in_stack_00000002 + 0x14) - iVar10) + iVar17;
              if (0x140 < iStack_2a) {
                iVar10 = iVar10 + iStack_2a + -0x140;
                iStack_2a = 0x140;
              }
              iVar17 = -((iStack_2a >> 1) + -0xa0);
              *(int *)(in_stack_00000002 + 0x10) = iVar17;
              *(int *)(in_stack_00000002 + 0x18) = iVar17;
              *(int *)(in_stack_00000002 + 0x1c) = iStack_2a;
              piVar5[2] = (iVar17 + *(int *)(in_stack_00000002 + 0x14)) - iVar10;
            }
          }
          else {
            in_stack_00000002[10] = in_stack_00000002[10] | 0x40;
          }
        }
      }
      iVar10 = *(int *)(in_stack_00000002 + 0x10);
      *(int *)(in_stack_00000002 + 0x24) = *(int *)(in_stack_00000002 + 0x24) + iVar10;
      iVar17 = *(int *)(in_stack_00000002 + 0x12);
      *(int *)(in_stack_00000002 + 0x26) = *(int *)(in_stack_00000002 + 0x26) + iVar17;
      *(int *)(in_stack_00000002 + 0x2a) = *(int *)(in_stack_00000002 + 0x2a) + iVar10;
      *(int *)(in_stack_00000002 + 0x2c) = *(int *)(in_stack_00000002 + 0x2c) + iVar17;
      *(int *)(in_stack_00000002 + 0x36) = *(int *)(in_stack_00000002 + 0x36) + iVar10;
      *(int *)(in_stack_00000002 + 0x38) = *(int *)(in_stack_00000002 + 0x38) + iVar17;
      in_stack_0000fff6 = (uint *)0x0;
    }
    return in_stack_0000fff6;
  case (uint *)0x17:
    return in_BX;
  case (uint *)0x18:
    FUN_133d_258e();
    FUN_1cc9_0310();
    *(undefined2 *)0x608 = 0;
    return (uint *)0x6c0a;
  case (uint *)0x19:
    pcVar7 = (code *)swi(0x33);
    (*pcVar7)();
    puVar24 = (uint *)FUN_1f65_055c();
    puVar11 = (uint *)puVar24;
    *(undefined2 *)0x5e22 = in_stack_00000002;
    *(undefined2 *)0x5e24 = (int)((ulong)puVar24 >> 0x10);
    if (*(char *)0x3c7e == '\0') {
      puVar11 = (uint *)FUN_1f65_0007();
    }
    else {
      while (*(char *)0x3c7e != *(char *)0x5ad2) {
        puVar11 = (uint *)FUN_1f65_004e();
      }
    }
    *(undefined2 *)0x3c87 = *(undefined2 *)0x3c85;
    *(undefined1 *)0x3c89 = 0;
    *(undefined2 *)0x3c83 = 0;
    return puVar11;
  case (uint *)0x1a:
    while( true ) {
      if ((int)puVar18 < 0) {
        unaff_DI = unaff_DI + -0x4000;
        unaff_ES = unaff_ES + 0x800;
      }
      in_BX = (uint *)((int)in_BX + -1);
      puVar18 = in_AX;
      if (in_BX == (uint *)0x0) break;
      for (; puVar18 != (uint *)0x0; puVar18 = (uint *)((int)puVar18 + -1)) {
        puVar3 = unaff_DI;
        unaff_DI = unaff_DI + 1;
        puVar24 = unaff_SI;
        unaff_SI = unaff_SI + 1;
        *puVar3 = *puVar24;
      }
      unaff_SI = unaff_SI + (int)puVar11;
      if ((int)unaff_SI < 0) {
        unaff_SI = unaff_SI + -0x4000;
        unaff_DS = unaff_DS + 0x800;
      }
      unaff_DI = (uint *)((int)unaff_DI + (int)pcStack_10);
      puVar18 = unaff_DI;
    }
    return puStack_e;
  case (uint *)0x1b:
    if ((puVar18[5] & 4) != 0) {
      *(int *)0x554 = (int)in_BX + -0x3a99;
      for (iStack_24 = (int)in_BX + -0x3a99; iStack_24 < (int)param_1[1]; iStack_24 = iStack_24 + 1)
      {
        puStack_e = (uint *)0x1000;
        pcStack_10 = (char *)0x6424;
        puStack_c = param_1;
        iVar10 = FUN_133d_0998();
        if (iVar10 != 0) {
          *(uint *)0x554 = *(uint *)0x554 | 1 << ((byte)iStack_24 & 0x1f);
        }
      }
    }
    FUN_1c21_002a();
    if (*(int *)0x570 != 0) {
      FUN_1000_0002();
    }
    return *(uint **)CONCAT22(param_2,param_1);
  case (uint *)0x1c:
    *(byte *)((int)unaff_SI + -0x47) = *(byte *)((int)unaff_SI + -0x47) & bVar19;
code_r0x0001010a:
    puVar11 = (uint *)*(int *)0x4e96;
    iVar10 = *(int *)0x4e94;
    puStack_c = (uint *)*(undefined2 *)0x4e92;
    do {
      pcStack_10 = (char *)0x134;
      puStack_e = (uint *)uVar21;
      FUN_1c76_0004();
      puStack_e = (uint *)0x1c76;
      do {
        do {
          iVar10 = iVar10 + 1;
          while (*(int *)0x4b14 <= iVar10) {
            puVar11 = (uint *)((int)puVar11 + 1);
            if (*(int *)0x4b12 <= (int)puVar11) {
              return puVar11;
            }
            iVar10 = 0;
          }
          puVar11 = (uint *)*(int *)0x4e96;
          iVar10 = *(int *)0x4e94;
          puStack_c = (uint *)*(undefined2 *)0x4e92;
          uVar21 = 0x1c78;
          pcStack_10 = (char *)0xf5;
          uVar8 = FUN_1c78_0000();
          uVar9 = uVar8 & 0x1f;
          puStack_e = (uint *)uVar21;
        } while (0x17 < uVar9);
        if ((uVar8 & 0x20) != 0) goto code_r0x0001010a;
      } while ((uVar9 < 0x10) || (0x17 < uVar9));
      puStack_c = (uint *)*(undefined2 *)0x4e92;
      puStack_e = (uint *)0x1c78;
      uVar21 = 0x1c78;
      pcStack_10 = (char *)0x151;
      FUN_1c78_0000();
      puVar11 = (uint *)*(int *)0x4e96;
      iVar10 = *(int *)0x4e94;
      puStack_c = (uint *)*(undefined2 *)0x4e92;
    } while( true );
  case (uint *)0x1d:
    puStack_c = param_1;
    puStack_e = (uint *)0x1000;
    pcStack_10 = (char *)0xcb1e;
    puVar11 = (uint *)FUN_203d_009e();
    return puVar11;
  case (uint *)0x1e:
    puStack_c = (uint *)0x1ba;
    puStack_e = (uint *)0x1000;
    pcStack_10 = (char *)0xcd7;
    FUN_18ad_024e();
    puVar11 = (uint *)FUN_1000_082c();
    return puVar11;
  case (uint *)0x1f:
    return in_BX;
  case (uint *)0x20:
                    // WARNING: Bad instruction - Truncating control flow here
    halt_baddata();
  case (uint *)0x21:
    puStack_c = (uint *)0x20b;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x226;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x241;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x25c;
    FUN_16d7_07de();
    puStack_c = (uint *)0x274;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x28f;
    FUN_16d7_07de();
    iVar10 = FUN_1842_001a();
    if (iVar10 != 0) {
      return (uint *)0x29d;
    }
    FUN_1842_0106();
    puStack_c = (uint *)0x2be;
    FUN_16d7_0642();
    FUN_1842_0106();
    puStack_c = (uint *)0x2d9;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x2f4;
    FUN_16d7_07de();
    puStack_c = (uint *)0x30c;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x327;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x342;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x35d;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x378;
    FUN_16d7_07de();
    puStack_c = (uint *)0x390;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x3ab;
    FUN_16d7_07de();
    iVar10 = FUN_1842_001a();
    if (iVar10 != 0) {
      return (uint *)0x3b9;
    }
    FUN_1842_0106();
    puStack_c = (uint *)0x3da;
    FUN_16d7_0642();
    FUN_1842_0106();
    puStack_c = (uint *)0x3f5;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x410;
    FUN_16d7_07de();
    puStack_c = (uint *)0x428;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x443;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x45e;
    FUN_16d7_07de();
    puStack_c = (uint *)0x476;
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)0x491;
    FUN_16d7_07de();
    iVar10 = FUN_1842_001a();
    if (iVar10 != 0) {
      return (uint *)0x49f;
    }
LAB_1000_04a9:
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x4c1,(uint)uStack_14);
    FUN_16d7_0642();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x4dc,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x4f7,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x512,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x52d,(uint)uStack_14);
    FUN_16d7_07de();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(int *)0x78;
    pcStack_10 = (char *)0x16d7;
    uStack_14 = (int *)CONCAT22(0x545,(uint)uStack_14);
    FUN_16d7_07de();
    goto switchD_1000_bb3f_caseD_3;
  case (uint *)0x22:
    do {
      puVar11 = in_AX;
      if (in_AX != (uint *)0x0) {
        for (; puVar11 != (uint *)0x0; puVar11 = (uint *)((int)puVar11 + -1)) {
          puVar3 = unaff_DI;
          unaff_DI = unaff_DI + 1;
          puVar24 = unaff_SI;
          unaff_SI = unaff_SI + 1;
          *puVar3 = *puVar24;
        }
      }
      *(char *)unaff_DI = (char)*unaff_SI;
      unaff_SI = (uint *)((int)unaff_SI + 1 + (int)puVar18);
      if ((int)unaff_SI < 0) {
        unaff_SI = unaff_SI + -0x4000;
        unaff_DS = unaff_DS + 0x800;
      }
      unaff_DI = (uint *)((int)unaff_DI + (int)pcStack_10 + 1);
      if ((int)unaff_DI < 0) {
        unaff_DI = unaff_DI + -0x4000;
        unaff_ES = unaff_ES + 0x800;
      }
      in_BX = (uint *)((int)in_BX + -1);
    } while (in_BX != (uint *)0x0);
    return puStack_e;
  }
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x93be,(uint)uStack_14);
  FUN_16d7_07de();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x16d7;
  uStack_14 = (int *)CONCAT22(0x93d6,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x93f2,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x940e,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x942a,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x9446,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x9462,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x947e,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(int *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x949a,(uint)uStack_14);
  FUN_16d7_07de();
switchD_1000_bb3f_caseD_7:
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x94b6,(uint)uStack_14);
  FUN_16d7_07de();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x16d7;
  uStack_14 = (int *)CONCAT22(0x94cf,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x94eb,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x9507,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x9523,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x953f,(uint)uStack_14);
  FUN_16d7_07de();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x16d7;
  uStack_14 = (int *)CONCAT22(0x9558,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x9574,(uint)uStack_14);
  FUN_16d7_07de();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x16d7;
  uStack_14 = (int *)CONCAT22(0x958c,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(undefined2 *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x95a8,(uint)uStack_14);
  FUN_16d7_07de();
  FUN_1842_0106();
  puStack_c = (uint *)*(undefined2 *)0x7a;
  puStack_e = (uint *)*(int *)0x78;
  pcStack_10 = (char *)0x1842;
  uStack_14 = (int *)CONCAT22(0x95c4,(uint)uStack_14);
  FUN_16d7_07de();
  iVar10 = FUN_1842_001a();
  if (iVar10 == 0) {
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x95f2,(uint)uStack_14);
    FUN_16d7_0642();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x960d,(uint)uStack_14);
    FUN_16d7_07de();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x16d7;
    uStack_14 = (int *)CONCAT22(0x9625,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9640,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x965b,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9676,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9691,(uint)uStack_14);
    FUN_16d7_07de();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x16d7;
    uStack_14 = (int *)CONCAT22(0x96a9,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x96c4,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x96df,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x96fa,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9715,(uint)uStack_14);
    FUN_16d7_07de();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(undefined2 *)0x78;
    pcStack_10 = (char *)0x16d7;
    uStack_14 = (int *)CONCAT22(0x972d,(uint)uStack_14);
    FUN_16d7_07de();
    FUN_1842_0106();
    puStack_c = (uint *)*(undefined2 *)0x7a;
    puStack_e = (uint *)*(int *)0x78;
    pcStack_10 = (char *)0x1842;
    uStack_14 = (int *)CONCAT22(0x9748,(uint)uStack_14);
    FUN_16d7_07de();
    iVar10 = FUN_1842_001a();
    if (iVar10 == 0) {
      FUN_1842_0106();
      puStack_c = (uint *)*(undefined2 *)0x7a;
      puStack_e = (uint *)*(undefined2 *)0x78;
      pcStack_10 = (char *)0x1842;
      uStack_14 = (int *)CONCAT22(0x9776,(uint)uStack_14);
      FUN_16d7_0642();
      FUN_1842_0106();
      puStack_c = (uint *)*(undefined2 *)0x7a;
      puStack_e = (uint *)*(int *)0x78;
      pcStack_10 = (char *)0x1842;
      uStack_14 = (int *)CONCAT22(0x9791,(uint)uStack_14);
      FUN_16d7_07de();
      FUN_1842_0106();
code_r0x000197a3:
      FUN_16d7_07de();
      FUN_1842_0106();
      FUN_16d7_07de();
      iVar10 = FUN_1842_001a();
      if (iVar10 == 0) {
        FUN_1842_0106();
        FUN_16d7_0642();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_16d7_07de();
        FUN_1842_0106();
        iVar10 = FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_1842_0106();
        FUN_16d7_07de();
        FUN_1842_0106();
        uVar21 = *(undefined2 *)0x7a;
        FUN_16d7_07de();
        *(undefined2 *)(iVar10 + 2) = 0x13;
        if ((*(byte *)0x5e2f & 0x20) == 0) {
          FUN_16d7_0522();
        }
        iVar10 = FUN_1842_001a();
        if (iVar10 == 0) {
          FUN_1842_0106();
          FUN_16d7_0642();
          FUN_1842_0106();
          FUN_16d7_07de();
          FUN_1842_0106();
          FUN_16d7_07de();
          FUN_1842_0106();
          FUN_16d7_07de();
          FUN_16d7_07de();
          FUN_1842_0106();
          FUN_16d7_07de();
          FUN_1842_0106();
          FUN_16d7_07de();
          FUN_1842_0106();
          FUN_16d7_07de();
          unaff_SI = (uint *)0x0;
        }
      }
    }
  }
LAB_18f9_0b03:
  FUN_1842_0000();
  return unaff_SI;
}



void __stdcall16far FUN_1bc4_0000(undefined2 param_1,undefined2 param_2)

{
  undefined1 *puVar1;
  int in_DX;
  
  puVar1 = (undefined1 *)FUN_2388_0d82(param_1,param_2,10);
  if (in_DX != 0 || puVar1 != (undefined1 *)0x0) {
    *puVar1 = 0;
  }
  return;
}



void __stdcall16far FUN_1bc4_002e(int param_1,undefined2 param_2)

{
  int iVar1;
  
  iVar1 = FUN_2388_0dd4(param_1,param_2);
  *(undefined1 *)(iVar1 + param_1) = 10;
  ((undefined1 *)(iVar1 + param_1))[1] = 0;
  return;
}



// WARNING: Removing unreachable block (ram,0x0001bd70)
// WARNING: Removing unreachable block (ram,0x0001be70)
// WARNING: Removing unreachable block (ram,0x0001bdfa)
// WARNING: Removing unreachable block (ram,0x0001be11)
// WARNING: Removing unreachable block (ram,0x0001bd07)
// WARNING: Removing unreachable block (ram,0x0001bdeb)

void __stdcall16far FUN_1bca_0000(int param_1,int param_2,uint param_3,int param_4)

{
  long lVar1;
  uint in_AX;
  uint uVar2;
  int iVar3;
  int in_DX;
  int *in_BX;
  undefined2 uVar4;
  undefined2 unaff_DS;
  bool bVar5;
  undefined1 local_110 [256];
  uint local_10;
  uint local_e;
  int local_c;
  uint local_a;
  int local_8;
  undefined4 local_6;
  
  local_6 = CONCAT22(in_DX,in_AX);
  uVar4 = 0x1bca;
  local_c = 0;
  local_e = 0;
  local_8 = 0;
  local_a = 0;
  if (in_DX != 0 || in_AX != 0) {
    if ((param_1 != 1) || (param_2 != 0)) {
      uVar4 = 0x2388;
      local_6 = FUN_2388_0bbc(param_1,param_2,in_AX,in_DX);
    }
    if (*(int *)0x718 == 0) {
      if (0 < in_BX[1]) {
        uVar2 = in_BX[1];
        if (local_6 < in_BX[1]) {
          uVar2 = (uint)local_6;
        }
        uVar4 = 0x2388;
        FUN_2388_0c4a(param_3,param_4,*in_BX);
        *in_BX = *in_BX + uVar2;
        in_BX[1] = in_BX[1] - uVar2;
        local_c = 0;
        local_8 = 0;
        bVar5 = CARRY2(param_3,uVar2);
        param_3 = param_3 + uVar2;
        param_4 = param_4 + (uint)bVar5 * 0x1000;
        local_e = uVar2;
        local_a = uVar2;
      }
      if ((in_BX[1] == 0) && ((*(byte *)(in_BX + 3) & 4) == 0)) {
        uVar4 = 0x2388;
        FUN_2388_08dc();
      }
      lVar1 = local_6;
      if (CONCAT22(local_8,local_a) < local_6) {
        do {
          local_6._2_2_ = (int)((ulong)lVar1 >> 0x10);
          local_6._0_2_ = (uint)lVar1;
          uVar2 = (uint)local_6 - local_e;
          if ((-1 < (int)((local_6._2_2_ - local_c) - (uint)((uint)local_6 < local_e))) &&
             ((local_6._2_2_ - local_c != (uint)((uint)local_6 < local_e) || (0xf000 < uVar2)))) {
            uVar2 = 0xf000;
          }
          local_6 = lVar1;
          iVar3 = FUN_2388_0af2(uVar4,(int)*(char *)((int)in_BX + 7),param_3,param_4,uVar2,&local_10
                               );
          if (iVar3 != 0) break;
          bVar5 = CARRY2(local_e,local_10);
          local_e = local_e + local_10;
          local_c = local_c + (uint)bVar5;
          bVar5 = CARRY2(param_3,local_10);
          param_3 = param_3 + local_10;
          param_4 = param_4 + (uint)bVar5 * 0x1000;
          bVar5 = CARRY2(local_a,uVar2);
          local_a = local_a + uVar2;
          local_8 = local_8 + (uint)bVar5;
          uVar4 = 0x2388;
          lVar1 = local_6;
        } while (CONCAT22(local_8,local_a) < local_6);
      }
    }
    else if (0 < local_6) {
      do {
        lVar1 = local_6 - CONCAT22(local_c,local_e);
        uVar2 = (uint)lVar1;
        if (0x100 < lVar1) {
          uVar2 = 0x100;
        }
        iVar3 = FUN_2388_03be(local_110,uVar2,1);
        if (iVar3 == 0) break;
        FUN_2388_0c4a(param_3,param_4,local_110);
        bVar5 = CARRY2(local_e,uVar2);
        local_e = local_e + uVar2;
        local_c = local_c + (uint)bVar5;
        bVar5 = CARRY2(param_3,uVar2);
        param_3 = param_3 + uVar2;
        param_4 = param_4 + (uint)bVar5 * 0x1000;
        bVar5 = CARRY2(local_a,uVar2);
        local_a = local_a + uVar2;
        local_8 = local_8 + (uint)bVar5;
      } while (CONCAT22(local_8,local_a) < local_6);
    }
    if ((local_e == in_AX) && (local_c == in_DX)) {
      return;
    }
    FUN_2388_0b22(local_e,local_c,in_AX,in_DX);
  }
  return;
}



// WARNING: Removing unreachable block (ram,0x0001bf0d)
// WARNING: Removing unreachable block (ram,0x0001bf7d)

void __stdcall16far FUN_1bea_0008(int param_1,int param_2,uint param_3,int param_4)

{
  uint in_AX;
  uint uVar1;
  int iVar2;
  int in_DX;
  int in_BX;
  undefined2 uVar3;
  undefined2 unaff_DS;
  bool bVar4;
  long lVar5;
  uint local_10;
  uint local_e;
  int local_c;
  uint local_a;
  int local_8;
  undefined4 local_6;
  
  lVar5 = CONCAT22(in_DX,in_AX);
  uVar3 = 0x1bea;
  local_c = 0;
  local_e = 0;
  local_8 = 0;
  local_a = 0;
  if (in_DX != 0 || in_AX != 0) {
    if ((*(byte *)(in_BX + 6) & 4) == 0) {
      uVar3 = 0x2388;
      FUN_2388_08dc();
    }
    if ((param_1 != 1) || (param_2 != 0)) {
      uVar3 = 0x2388;
      lVar5 = FUN_2388_0bbc(in_AX,in_DX,param_1,param_2);
    }
    local_6 = lVar5;
    if (0 < lVar5) {
      do {
        local_6._2_2_ = (int)((ulong)lVar5 >> 0x10);
        local_6._0_2_ = (uint)lVar5;
        uVar1 = (uint)local_6 - local_e;
        if ((-1 < (int)((local_6._2_2_ - local_c) - (uint)((uint)local_6 < local_e))) &&
           ((local_6._2_2_ - local_c != (uint)((uint)local_6 < local_e) || (0xf000 < uVar1)))) {
          uVar1 = 0xf000;
        }
        local_6 = lVar5;
        iVar2 = FUN_2388_0af9(uVar3,(int)*(char *)(in_BX + 7),param_3,param_4,uVar1,&local_10);
        if (iVar2 != 0) break;
        bVar4 = CARRY2(local_e,local_10);
        local_e = local_e + local_10;
        local_c = local_c + (uint)bVar4;
        bVar4 = CARRY2(param_3,local_10);
        param_3 = param_3 + local_10;
        param_4 = param_4 + (uint)bVar4 * 0x1000;
        bVar4 = CARRY2(local_a,uVar1);
        local_a = local_a + uVar1;
        local_8 = local_8 + (uint)bVar4;
        uVar3 = 0x2388;
        lVar5 = local_6;
      } while (CONCAT22(local_8,local_a) < local_6);
    }
    if ((local_e == in_AX) && (local_c == in_DX)) {
      return;
    }
    FUN_2388_0b22(local_e,local_c,in_AX,in_DX);
  }
  return;
}



// WARNING: Globals starting with '_' overlap smaller symbols at the same address

undefined2 __cdecl16far FUN_1bfb_001c(char *param_1)

{
  char *pcVar1;
  char cVar2;
  code *pcVar3;
  int iVar4;
  char *pcVar5;
  undefined2 unaff_DS;
  undefined1 uVar6;
  undefined2 *puStack_ae;
  undefined2 local_a6;
  char local_55 [81];
  undefined2 local_4;
  
  pcVar5 = local_55;
  iVar4 = 0x4f;
  do {
    pcVar1 = param_1;
    param_1 = param_1 + 1;
    cVar2 = *pcVar1;
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 1;
    *pcVar1 = cVar2;
    iVar4 = iVar4 + -1;
  } while (iVar4 != 0 && cVar2 != '\0');
  pcVar5 = local_55;
  puStack_ae = &local_a6;
  FUN_1297_00ba();
  uVar6 = (undefined1 *)0xfff7 < &puStack_ae;
  pcVar3 = (code *)swi(0x21);
  (*pcVar3)();
  local_a6 = 0x1000;
  pcVar3 = (code *)swi(0x21);
  _caseD_1f = pcVar5;
  _FUN_1000_0002 = unaff_DS;
  (*pcVar3)();
  pcVar3 = (code *)swi(0x21);
  (*pcVar3)();
  if ((bool)uVar6) {
    local_4 = 0;
  }
  else {
    pcVar3 = (code *)swi(0x21);
    (*pcVar3)();
    local_4 = 0xffff;
  }
  pcVar3 = (code *)swi(0x21);
  (*pcVar3)();
  return local_4;
}



void __stdcall16far
FUN_1c04_000e(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  int iVar1;
  int in_DX;
  
  iVar1 = FUN_2388_0d82(param_3,param_4,0x2e);
  if (in_DX == 0 && iVar1 == 0) {
    FUN_2388_0e22(param_3,param_4,0x71a);
    FUN_2388_0e22(param_3,param_4,param_1,param_2);
  }
  FUN_2388_0db0(param_3,param_4);
  return;
}



void __stdcall16far
FUN_1c04_005c(undefined2 param_1,undefined2 param_2,int param_3,int param_4,int param_5,int param_6)

{
  undefined1 *puVar1;
  int iVar2;
  
  iVar2 = param_6;
  if ((param_3 != param_5) || (param_4 != param_6)) {
    FUN_2388_0dec(param_5,param_6,param_3,param_4);
  }
  puVar1 = (undefined1 *)FUN_2388_0d82(param_5,param_6,0x2e);
  if (iVar2 != 0 || puVar1 != (undefined1 *)0x0) {
    *puVar1 = 0;
  }
  FUN_2388_0e22(param_5,param_6,0x71c);
  FUN_2388_0e22(param_5,param_6,param_1,param_2);
  FUN_2388_0db0(param_5,param_6);
  return;
}



undefined4 __stdcall16far
FUN_1c11_0004(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5,undefined2 param_6)

{
  char *pcVar1;
  undefined2 unaff_SS;
  char local_56 [82];
  
  FUN_2388_0dec(local_56);
  pcVar1 = local_56;
  while (local_56[0] != '\0') {
    pcVar1 = pcVar1 + 1;
    local_56[0] = *pcVar1;
  }
  if (pcVar1[-1] != '\\') {
    FUN_2388_05e6(local_56,0x71e);
  }
  FUN_2388_0dec(param_5,param_6,local_56);
  FUN_2388_0e22(param_5,param_6,param_1,param_2);
  FUN_2388_0db0(param_5,param_6);
  return CONCAT22(param_6,param_5);
}



void __stdcall16far FUN_1c1a_0002(char *param_1,undefined2 param_2)

{
  char cVar1;
  char *pcVar2;
  undefined1 local_106 [258];
  undefined2 local_4;
  
  FUN_201f_000c(param_1,param_2);
  local_4 = param_2;
  cVar1 = *param_1;
  pcVar2 = param_1;
  while ((cVar1 != '\0' && ((*pcVar2 == ' ' || (*pcVar2 == '\t'))))) {
    pcVar2 = pcVar2 + 1;
    cVar1 = *pcVar2;
  }
  FUN_2388_0dec(local_106);
  FUN_2388_0dec(param_1,param_2,local_106);
  return;
}



void __cdecl16far FUN_1c21_002a(void)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x734 = 0xffff;
  *(undefined2 *)0x736 = 0xffff;
  *(undefined2 *)0x72a = *(undefined2 *)0x722;
  *(undefined2 *)0x72e = 0;
  *(undefined2 *)0x728 = 0;
  return;
}



void __cdecl16far FUN_1c21_0042(void)

{
  uint uVar1;
  int in_AX;
  undefined2 uVar2;
  undefined2 in_DX;
  int iVar3;
  undefined2 unaff_DS;
  
  if (in_AX != 0) {
    FUN_1f65_038a();
  }
  *(undefined2 *)0x734 = *(undefined2 *)0x724;
  *(undefined2 *)0x736 = *(undefined2 *)0x726;
  uVar2 = FUN_1f65_057c(0x724,0x726);
  *(undefined2 *)0x722 = uVar2;
  uVar2 = FUN_1d18_0006();
  *(undefined2 *)0x738 = uVar2;
  *(undefined2 *)0x73a = in_DX;
  uVar1 = *(uint *)0x722;
  if ((*(int *)0x72e == 0) || (uVar1 != 0)) {
    *(undefined2 *)0x730 = 0;
  }
  else {
    *(undefined2 *)0x730 = 1;
  }
  if ((uVar1 == 0) || (*(int *)0x72a != 0)) {
    iVar3 = 0;
  }
  else {
    iVar3 = 1;
  }
  *(uint *)0x72a = uVar1;
  if (uVar1 == 0) {
    *(undefined2 *)0x72e = 0;
  }
  if ((((*(int *)0x734 == *(int *)0x724) && (*(int *)0x736 == *(int *)0x726)) && (iVar3 == 0)) &&
     (*(int *)0x730 == 0)) {
    *(undefined2 *)0x72c = 0;
  }
  else {
    *(undefined2 *)0x72c = 1;
  }
  *(int *)0x728 = iVar3;
  if (iVar3 != 0) {
    *(undefined2 *)0x72e = 0xffff;
    *(uint *)0x720 = (uint)((uVar1 & 1) == 0);
  }
  if ((uVar1 == 0) && (*(int *)0x730 == 0)) {
    *(undefined2 *)0x732 = 0;
    return;
  }
  *(undefined2 *)0x732 = 1;
  return;
}



void __cdecl16far FUN_1c21_0110(void)

{
  int in_AX;
  int iVar1;
  int in_DX;
  int iVar2;
  undefined2 unaff_DS;
  
  iVar2 = in_DX;
  if (in_AX != 0) {
    FUN_1f65_02c3(0xffff);
  }
  if (in_DX != 0) {
    do {
      iVar1 = FUN_1d18_0006();
      if (iVar1 != *(int *)0x738) {
        return;
      }
    } while (iVar2 == *(int *)0x73a);
  }
  return;
}



void __cdecl16far FUN_1c34_000c(void)

{
  undefined2 unaff_DS;
  
  FUN_1f65_075e(*(undefined2 *)0x3af8,*(undefined2 *)0x3afa,*(undefined2 *)0x3af6);
  FUN_1f65_0777(0,0,*(int *)0x3af6 + -1,*(int *)0x3af4 + -1);
  FUN_1f65_07a8(0,0);
  return;
}



void __stdcall16far FUN_1c34_0044(void)

{
  int iVar1;
  
  FUN_1f65_04bf();
  iVar1 = FUN_1f65_07cd();
  FUN_1ec5_0024(0x3af4);
  if (iVar1 != 0) {
    FUN_1f65_0903(0x1ec5);
  }
  FUN_1f65_04d1();
  return;
}



void __cdecl16far FUN_1c3c_000a(void)

{
  int in_AX;
  
  if (in_AX == 7) {
    DAT_0000_0410 = DAT_0000_0410 & 0xcf | 0x30;
    return;
  }
  DAT_0000_0410 = DAT_0000_0410 & 0xcf | 0x20;
  return;
}



undefined2 __cdecl16far FUN_1c3e_000a(void)

{
  uint in_AX;
  uint uVar1;
  uint in_DX;
  uint uVar2;
  uint *in_BX;
  undefined2 unaff_DS;
  
  uVar2 = (uint)((ulong)in_DX * (ulong)in_AX >> 0x10);
  uVar1 = FUN_1cc9_02e2();
  in_BX[2] = uVar1;
  in_BX[3] = uVar2;
  if (uVar2 == 0 && in_BX[2] == 0) {
    return 0;
  }
  in_BX[1] = in_AX;
  *in_BX = in_DX;
  return 0xffff;
}



undefined2 __stdcall16far FUN_1c3e_0044(undefined2 param_1)

{
  undefined2 in_AX;
  undefined2 in_DX;
  int iVar1;
  undefined2 *in_BX;
  undefined2 unaff_DS;
  undefined4 uVar2;
  
  uVar2 = FUN_1cc9_0136(param_1,unaff_DS);
  iVar1 = (int)((ulong)uVar2 >> 0x10);
  in_BX[2] = (int)uVar2;
  in_BX[3] = iVar1;
  if (iVar1 == 0 && in_BX[2] == 0) {
    return 0;
  }
  in_BX[1] = in_AX;
  *in_BX = in_DX;
  return 0xffff;
}



undefined2 __cdecl16far FUN_1c46_0006(void)

{
  undefined2 *in_BX;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  if (in_BX[3] != 0 || in_BX[2] != 0) {
    FUN_1cc9_0310(in_BX[2],in_BX[3]);
    local_4 = 0xffff;
  }
  in_BX[3] = 0;
  in_BX[2] = 0;
  in_BX[1] = 0;
  *in_BX = 0;
  return local_4;
}



void __stdcall16far
FUN_1c49_000e(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  undefined2 in_AX;
  
  FUN_1c5b_0004(in_AX,param_1,param_1,param_2,param_3,param_4);
  return;
}



int __stdcall16far
FUN_1c4c_0000(uint param_1,undefined2 param_2,int param_3,int param_4,int param_5,undefined2 param_6
             ,int param_7,int param_8,int param_9)

{
  undefined2 uVar1;
  undefined2 uVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  uint uVar6;
  undefined2 in_DX;
  uint uVar7;
  uint in_BX;
  undefined2 *puVar8;
  undefined2 *puVar9;
  undefined2 *puVar10;
  undefined2 *puVar11;
  int local_e;
  
  if ((param_9 == 0 && param_8 == 0) || (param_5 == 0 && param_4 == 0)) {
    local_e = 0;
  }
  else {
    local_e = 1;
  }
  if (local_e != 0) {
    uVar2 = in_DX;
    uVar1 = FUN_1c91_0000();
    puVar10 = (undefined2 *)FUN_204f_0002(uVar1,uVar2);
    uVar2 = FUN_1c91_0000();
    puVar11 = (undefined2 *)FUN_204f_0002(uVar2,in_DX);
    if (param_1 != 0) {
      uVar7 = in_BX >> 1;
      uVar3 = uVar7;
      if ((in_BX & 1) == 0) {
        while( true ) {
          iVar5 = (int)((ulong)puVar11 >> 0x10);
          puVar9 = (undefined2 *)puVar11;
          iVar4 = (int)((ulong)puVar10 >> 0x10);
          puVar8 = (undefined2 *)puVar10;
          uVar6 = uVar7;
          if (uVar3 == 0) break;
          for (; uVar6 != 0; uVar6 = uVar6 - 1) {
            puVar11 = puVar9;
            puVar9 = puVar9 + 1;
            puVar10 = puVar8;
            puVar8 = puVar8 + 1;
            *puVar11 = *puVar10;
          }
          puVar8 = (undefined2 *)((int)puVar8 + -(in_BX - param_7));
          if ((int)puVar8 < 0) {
            puVar8 = puVar8 + -0x4000;
            iVar4 = iVar4 + 0x800;
          }
          puVar10 = (undefined2 *)CONCAT22(iVar4,puVar8);
          puVar9 = (undefined2 *)((int)puVar9 + -(in_BX - param_3));
          if ((int)puVar9 < 0) {
            puVar9 = puVar9 + -0x4000;
            iVar5 = iVar5 + 0x800;
          }
          puVar11 = (undefined2 *)CONCAT22(iVar5,puVar9);
          param_1 = param_1 - 1;
          uVar3 = param_1;
        }
      }
      else {
        do {
          iVar5 = (int)((ulong)puVar11 >> 0x10);
          puVar9 = (undefined2 *)puVar11;
          iVar4 = (int)((ulong)puVar10 >> 0x10);
          puVar8 = (undefined2 *)puVar10;
          uVar3 = uVar7;
          if (uVar7 != 0) {
            for (; uVar3 != 0; uVar3 = uVar3 - 1) {
              puVar11 = puVar9;
              puVar9 = puVar9 + 1;
              puVar10 = puVar8;
              puVar8 = puVar8 + 1;
              *puVar11 = *puVar10;
            }
          }
          *(undefined1 *)puVar9 = *(undefined1 *)puVar8;
          puVar8 = (undefined2 *)((int)puVar8 + -(in_BX - param_7) + 1);
          if ((int)puVar8 < 0) {
            puVar8 = puVar8 + -0x4000;
            iVar4 = iVar4 + 0x800;
          }
          puVar10 = (undefined2 *)CONCAT22(iVar4,puVar8);
          puVar9 = (undefined2 *)((int)puVar9 + -(in_BX - param_3) + 1);
          if ((int)puVar9 < 0) {
            puVar9 = puVar9 + -0x4000;
            iVar5 = iVar5 + 0x800;
          }
          puVar11 = (undefined2 *)CONCAT22(iVar5,puVar9);
          param_1 = param_1 - 1;
        } while (param_1 != 0);
      }
    }
  }
  return local_e;
}



uint __stdcall16far
FUN_1c5b_0004(undefined1 param_1,uint param_2,undefined2 param_3,int param_4,int param_5,int param_6
             )

{
  int iVar1;
  undefined2 uVar2;
  uint uVar3;
  uint uVar4;
  undefined2 in_DX;
  uint uVar5;
  uint in_BX;
  undefined2 *puVar6;
  undefined2 *puVar7;
  uint local_c;
  uint local_a;
  int local_4;
  
  local_c = in_BX;
  iVar1 = FUN_1c91_0044(&param_2,&local_c);
  if (iVar1 == 0) {
    local_4 = param_4 - local_c;
    local_a = (uint)(param_6 != 0 || param_5 != 0);
    if (local_a != 0) {
      uVar2 = FUN_1c91_0000();
      puVar7 = (undefined2 *)FUN_204f_0002(uVar2,in_DX);
      if (param_2 != 0) {
        uVar5 = local_c >> 1;
        uVar3 = uVar5;
        if ((local_c & 1) == 0) {
          while( true ) {
            iVar1 = (int)((ulong)puVar7 >> 0x10);
            puVar6 = (undefined2 *)puVar7;
            uVar4 = uVar5;
            if (uVar3 == 0) break;
            for (; uVar4 != 0; uVar4 = uVar4 - 1) {
              puVar7 = puVar6;
              puVar6 = puVar6 + 1;
              *puVar7 = CONCAT11(param_1,param_1);
            }
            puVar6 = (undefined2 *)((int)puVar6 + local_4);
            if ((int)puVar6 < 0) {
              puVar6 = puVar6 + -0x4000;
              iVar1 = iVar1 + 0x800;
            }
            puVar7 = (undefined2 *)CONCAT22(iVar1,puVar6);
            param_2 = param_2 - 1;
            uVar3 = param_2;
          }
        }
        else {
          do {
            iVar1 = (int)((ulong)puVar7 >> 0x10);
            puVar6 = (undefined2 *)puVar7;
            uVar3 = uVar5;
            if (uVar5 != 0) {
              for (; uVar3 != 0; uVar3 = uVar3 - 1) {
                puVar7 = puVar6;
                puVar6 = puVar6 + 1;
                *puVar7 = CONCAT11(param_1,param_1);
              }
            }
            *(undefined1 *)puVar6 = param_1;
            puVar6 = (undefined2 *)((int)puVar6 + local_4 + 1);
            if ((int)puVar6 < 0) {
              puVar6 = puVar6 + -0x4000;
              iVar1 = iVar1 + 0x800;
            }
            puVar7 = (undefined2 *)CONCAT22(iVar1,puVar6);
            param_2 = param_2 - 1;
          } while (param_2 != 0);
        }
      }
    }
  }
  return local_a;
}



int __stdcall16far
FUN_1c67_0000(uint param_1,uint param_2,undefined2 param_3,undefined2 param_4,int param_5,
             int param_6,int param_7,undefined2 param_8,int param_9,int param_10,int param_11)

{
  undefined2 uVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  undefined2 in_DX;
  uint uVar6;
  undefined2 *puVar7;
  undefined2 *puVar8;
  undefined2 *puVar9;
  undefined2 *puVar10;
  int local_e;
  
  if ((param_11 == 0 && param_10 == 0) || (param_7 == 0 && param_6 == 0)) {
    local_e = 0;
  }
  else {
    local_e = 1;
  }
  if (local_e != 0) {
    uVar1 = FUN_1c91_0000();
    puVar9 = (undefined2 *)FUN_204f_0002(uVar1,in_DX);
    uVar1 = FUN_1c91_0000();
    puVar10 = (undefined2 *)FUN_204f_0002(uVar1,param_3);
    if (param_1 != 0) {
      uVar6 = param_2 >> 1;
      uVar2 = uVar6;
      if ((param_2 & 1) == 0) {
        while( true ) {
          iVar4 = (int)((ulong)puVar10 >> 0x10);
          puVar8 = (undefined2 *)puVar10;
          iVar3 = (int)((ulong)puVar9 >> 0x10);
          puVar7 = (undefined2 *)puVar9;
          uVar5 = uVar6;
          if (uVar2 == 0) break;
          for (; uVar5 != 0; uVar5 = uVar5 - 1) {
            puVar10 = puVar8;
            puVar8 = puVar8 + 1;
            puVar9 = puVar7;
            puVar7 = puVar7 + 1;
            *puVar10 = *puVar9;
          }
          puVar7 = (undefined2 *)((int)puVar7 + (param_9 - param_2));
          if ((int)puVar7 < 0) {
            puVar7 = puVar7 + -0x4000;
            iVar3 = iVar3 + 0x800;
          }
          puVar9 = (undefined2 *)CONCAT22(iVar3,puVar7);
          puVar8 = (undefined2 *)((int)puVar8 + (param_5 - param_2));
          if ((int)puVar8 < 0) {
            puVar8 = puVar8 + -0x4000;
            iVar4 = iVar4 + 0x800;
          }
          puVar10 = (undefined2 *)CONCAT22(iVar4,puVar8);
          param_1 = param_1 - 1;
          uVar2 = param_1;
        }
      }
      else {
        do {
          iVar4 = (int)((ulong)puVar10 >> 0x10);
          puVar8 = (undefined2 *)puVar10;
          iVar3 = (int)((ulong)puVar9 >> 0x10);
          puVar7 = (undefined2 *)puVar9;
          uVar2 = uVar6;
          if (uVar6 != 0) {
            for (; uVar2 != 0; uVar2 = uVar2 - 1) {
              puVar10 = puVar8;
              puVar8 = puVar8 + 1;
              puVar9 = puVar7;
              puVar7 = puVar7 + 1;
              *puVar10 = *puVar9;
            }
          }
          *(undefined1 *)puVar8 = *(undefined1 *)puVar7;
          puVar7 = (undefined2 *)((int)puVar7 + (param_9 - param_2) + 1);
          if ((int)puVar7 < 0) {
            puVar7 = puVar7 + -0x4000;
            iVar3 = iVar3 + 0x800;
          }
          puVar9 = (undefined2 *)CONCAT22(iVar3,puVar7);
          puVar8 = (undefined2 *)((int)puVar8 + (param_5 - param_2) + 1);
          if ((int)puVar8 < 0) {
            puVar8 = puVar8 + -0x4000;
            iVar4 = iVar4 + 0x800;
          }
          puVar10 = (undefined2 *)CONCAT22(iVar4,puVar8);
          param_1 = param_1 - 1;
        } while (param_1 != 0);
      }
    }
  }
  return local_e;
}



void __stdcall16far FUN_1c76_0004(void)

{
  undefined1 *puVar1;
  undefined2 in_DX;
  undefined1 in_BL;
  
  puVar1 = (undefined1 *)FUN_1c91_0000();
  *puVar1 = in_BL;
  return;
}



undefined1 __stdcall16far FUN_1c78_0000(void)

{
  undefined1 *puVar1;
  undefined2 in_DX;
  
  puVar1 = (undefined1 *)FUN_1c91_0000();
  return *puVar1;
}



void __stdcall16far
FUN_1c79_0006(undefined1 param_1,int param_2,int param_3,int param_4,undefined2 param_5)

{
  int in_AX;
  int iVar1;
  int in_DX;
  int in_BX;
  undefined1 *puVar2;
  
  if ((-1 < in_BX) && (in_BX < param_2)) {
    if (in_AX < 0) {
      in_AX = 0;
    }
    iVar1 = param_3 + -1;
    if (in_DX < param_3 + -1) {
      iVar1 = in_DX;
    }
    puVar2 = (undefined1 *)(param_3 * in_BX + in_AX + param_4);
    iVar1 = (iVar1 - in_AX) + 1;
    do {
      *puVar2 = param_1;
      puVar2 = puVar2 + 1;
      iVar1 = iVar1 + -1;
    } while (iVar1 != 0 && puVar2 != (undefined1 *)0x0);
  }
  return;
}



void __stdcall16far
FUN_1c80_0000(undefined1 param_1,int param_2,int param_3,undefined2 param_4,undefined2 param_5)

{
  int in_AX;
  undefined1 *puVar1;
  int iVar2;
  int in_DX;
  int in_BX;
  
  if ((-1 < in_AX) && (in_AX < param_3)) {
    if (in_DX < 0) {
      in_DX = 0;
    }
    iVar2 = param_2 + -1;
    if (in_BX < param_2 + -1) {
      iVar2 = in_BX;
    }
    puVar1 = (undefined1 *)(param_3 * in_DX + in_AX);
    iVar2 = (iVar2 - in_DX) + 1;
    do {
      *puVar1 = param_1;
      puVar1 = puVar1 + param_3;
      iVar2 = iVar2 + -1;
    } while (iVar2 != 0 && puVar1 != (undefined1 *)0x0);
  }
  return;
}



void __stdcall16far
FUN_1c86_000c(undefined1 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5,undefined2 param_6)

{
  undefined1 extraout_AH;
  undefined1 extraout_AH_00;
  undefined1 extraout_AH_01;
  
  FUN_1c79_0006(param_1,param_3,param_4,param_5,param_6);
  FUN_1c79_0006(CONCAT11(extraout_AH,param_1),param_3,param_4,param_5,param_6);
  FUN_1c80_0000(CONCAT11(extraout_AH_00,param_1),param_3,param_4,param_5,param_6);
  FUN_1c80_0000(CONCAT11(extraout_AH_01,param_1),param_3,param_4,param_5,param_6);
  return;
}



uint __cdecl16far FUN_1c91_0000(void)

{
  int in_AX;
  int in_DX;
  int in_BX;
  undefined2 unaff_DS;
  
  return *(int *)(in_BX + 2) * in_DX + in_AX + *(int *)(in_BX + 4) & 0xf;
}



undefined2 __stdcall16far FUN_1c91_0044(int *param_1,int *param_2)

{
  int *in_AX;
  int iVar1;
  int iVar2;
  int *in_DX;
  int *in_BX;
  undefined2 unaff_DS;
  
  if (*in_AX < 0) {
    *param_2 = *param_2 + *in_AX;
    *in_AX = 0;
  }
  if (*in_DX < 0) {
    *param_1 = *param_1 + *in_DX;
    *in_DX = 0;
  }
  iVar1 = *param_2 + *in_AX + -1;
  if (in_BX[1] + -1 < iVar1) {
    iVar1 = in_BX[1] + -1;
  }
  iVar2 = *param_1 + *in_DX + -1;
  if (*in_BX + -1 < iVar2) {
    iVar2 = *in_BX + -1;
  }
  *param_2 = (iVar1 - *in_AX) + 1;
  iVar1 = (iVar2 - *in_DX) + 1;
  *param_1 = iVar1;
  if ((0 < *param_2) && (0 < iVar1)) {
    return 0;
  }
  return 1;
}



int __stdcall16far
FUN_1c9d_000e(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  int in_AX;
  int iVar1;
  undefined2 *in_BX;
  undefined2 unaff_DS;
  int local_4;
  
  local_4 = -3;
  iVar1 = FUN_1c91_0044(&param_1,&param_2);
  if (iVar1 == 0) {
    if ((in_AX == -8) && (FUN_1c3e_0044(0x73c), *(int *)0x49a4 != 0 || *(int *)0x49a2 != 0)) {
      FUN_1c67_0000(param_1,param_2,0,*(undefined2 *)0x499e,*(undefined2 *)0x49a0,
                    *(undefined2 *)0x49a2,*(undefined2 *)0x49a4,*in_BX,in_BX[1],in_BX[2],in_BX[3]);
      local_4 = -1;
    }
    else {
      local_4 = FUN_203d_000a(param_1,param_2,param_3,param_4);
      if (local_4 < 0) {
        if ((in_AX == -2) || (iVar1 = FUN_2025_000c(param_1,param_2), iVar1 < 0)) {
          local_4 = -3;
        }
        else {
          local_4 = -(iVar1 + 10);
        }
      }
    }
  }
  return local_4;
}



void __stdcall16far
FUN_1c9d_00ec(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  int in_AX;
  int iVar1;
  undefined2 *in_BX;
  undefined2 unaff_DS;
  
  iVar1 = FUN_1c91_0044(&param_1,&param_2);
  if (iVar1 == 0) {
    if (in_AX < -0x13) {
LAB_1c9d_0136:
      FUN_203d_009e(param_1,param_2,param_3,param_4);
      return;
    }
    if (in_AX == -10 || in_AX + 0x13 < 9) {
      FUN_2025_00ea(param_1,param_2,param_3,param_4);
      return;
    }
    if (in_AX != -3) {
      if (in_AX != -1) goto LAB_1c9d_0136;
      FUN_1c67_0000(param_1,param_2,param_3,*in_BX,in_BX[1],in_BX[2],in_BX[3],*(undefined2 *)0x499e,
                    *(undefined2 *)0x49a0,*(undefined2 *)0x49a2,*(undefined2 *)0x49a4);
      if (*(int *)0x746 == 0) {
        FUN_1c46_0006();
      }
    }
  }
  return;
}



void __cdecl16far
FUN_1cb9_0000(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             int param_5,int param_6,undefined2 param_7,undefined2 param_8,int param_9,int param_10,
             int param_11,int param_12,int param_13,int param_14)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int local_a;
  int local_6;
  
  if ((param_6 != 0) && (param_5 != 0)) {
    uVar1 = param_9 - param_13;
    if ((int)uVar1 < 1) {
      uVar1 = ~uVar1 + 1;
    }
    uVar2 = param_10 - param_14;
    if ((int)uVar2 < 1) {
      uVar2 = ~uVar2 + 1;
    }
    local_a = (int)uVar2 % param_5;
    param_11 = param_9 + param_11;
    param_12 = param_10 + param_12;
    for (; local_6 = (int)uVar1 % param_6, iVar6 = param_9, param_10 < param_12;
        param_10 = param_10 + iVar6) {
      while (iVar6 < param_11) {
        iVar3 = (iVar6 - local_6) + param_6;
        iVar4 = iVar3;
        if (param_11 < iVar3) {
          iVar4 = param_11;
        }
        iVar5 = (param_10 - local_a) + param_5;
        if (param_12 < iVar5) {
          iVar5 = param_12;
        }
        FUN_1c67_0000(iVar5 - param_10,iVar4 - iVar6,param_10,param_1,param_2,param_3,param_4,
                      param_5,param_6,param_7,param_8);
        local_6 = 0;
        iVar6 = iVar3;
      }
      iVar6 = param_5 - local_a;
      local_a = 0;
    }
  }
  return;
}



int __cdecl16far FUN_1cc9_0004(uint param_1)

{
  uint uVar1;
  code *pcVar2;
  int iVar3;
  int in_BX;
  int unaff_SI;
  int iVar4;
  undefined2 unaff_ES;
  
  pcVar2 = (code *)swi(0x21);
  (*pcVar2)();
  pcVar2 = (code *)swi(0x21);
  iVar3 = in_BX;
  (*pcVar2)();
  iVar3 = *(int *)(iVar3 + -2);
  do {
    if (*(int *)0x1 == 0) {
      iVar4 = iVar3;
      if (unaff_SI != 0) {
        iVar4 = *(int *)0x3;
        *(undefined1 *)0x0 = *(undefined1 *)0x0;
        *(int *)0x3 = *(int *)0x3 + iVar4;
        *(int *)0x3 = *(int *)0x3 + 1;
        iVar4 = unaff_SI;
      }
      uVar1 = *(uint *)0x3;
      iVar3 = iVar4;
      if (param_1 <= uVar1) {
        if (uVar1 != param_1) {
          iVar3 = iVar4 + param_1 + 1;
          *(int *)0x3 = (uVar1 - param_1) + -1;
          *(undefined2 *)0x1 = 0;
          *(undefined1 *)0x0 = *(undefined1 *)0x0;
          *(undefined1 *)0x0 = 0x4d;
          *(uint *)0x3 = param_1;
        }
        *(int *)0x1 = in_BX;
        return iVar4 + 1;
      }
    }
    else {
      iVar4 = 0;
    }
    if (*(char *)0x0 == 'Z') {
      return 0;
    }
    iVar3 = iVar3 + *(int *)0x3 + 1;
    unaff_SI = iVar4;
  } while( true );
}



void __cdecl16far FUN_1cc9_009c(void)

{
  code *pcVar1;
  int iVar2;
  int in_BX;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  int iVar3;
  int iVar4;
  
  iVar3 = 0;
  *(undefined1 *)0x74b = 0;
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  for (iVar2 = *(int *)(in_BX + -2);
      (iVar4 = iVar3, *(char *)0x0 != 'Z' && (iVar4 = iVar2, *(char *)0x0 == 'M'));
      iVar2 = iVar2 + *(int *)0x3 + 1) {
  }
  if (iVar4 != 0) {
    FUN_1ed0_03d6(0,0,iVar4,0);
  }
  return;
}



void __cdecl16far FUN_1cc9_00f0(code *param_1,int param_2)

{
  undefined2 unaff_DS;
  
  if (param_1 != (code *)0x0 || param_2 != 0) {
    *(undefined2 *)0x76e = 0xffff;
    (*param_1)(0x1cc9);
    *(undefined2 *)0x76e = 0;
  }
  return;
}



void __cdecl16far FUN_1cc9_010c(undefined2 param_1,int param_2,char *param_3)

{
  char *pcVar1;
  char cVar2;
  undefined2 *puVar3;
  int iVar4;
  char *pcVar5;
  undefined2 *puVar6;
  char *pcVar7;
  
  pcVar5 = (char *)param_3;
  puVar6 = (undefined2 *)0x8;
  for (iVar4 = 4; iVar4 != 0; iVar4 = iVar4 + -1) {
    puVar3 = puVar6;
    puVar6 = puVar6 + 1;
    *puVar3 = 0;
  }
  pcVar7 = (char *)0x8;
  iVar4 = 8;
  do {
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 1;
    cVar2 = *pcVar1;
    pcVar1 = pcVar7;
    pcVar7 = pcVar7 + 1;
    *pcVar1 = cVar2;
    iVar4 = iVar4 + -1;
  } while (iVar4 != 0 && cVar2 != '\0');
  return;
}



undefined4 __stdcall16far FUN_1cc9_0136(undefined2 param_1,undefined2 param_2)

{
  uint in_AX;
  undefined2 uVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  uint in_DX;
  undefined2 unaff_DS;
  uint local_a;
  uint local_8;
  
  local_8 = 0;
  local_a = 0;
  if ((0 < (int)in_DX) || ((-1 < (int)in_DX && (in_AX != 0)))) {
    uVar3 = in_DX;
    uVar1 = FUN_1d08_0068();
    uVar4 = uVar3;
    uVar2 = FUN_227e_000c();
    *(uint *)0x74e = in_AX;
    *(uint *)0x750 = in_DX;
    *(undefined2 *)0x752 = uVar1;
    *(uint *)0x754 = uVar3;
    *(uint *)0x756 = uVar2;
    *(uint *)0x758 = uVar4;
    if (*(char *)0x749 == '\0') {
      *(undefined2 *)0x75a = uVar1;
      *(uint *)0x75c = uVar3;
      *(undefined2 *)0x75e = uVar1;
      *(uint *)0x760 = uVar3;
      *(undefined1 *)0x749 = 0xff;
    }
    if (*(char *)0x74a == '\0') {
      *(uint *)0x762 = uVar2;
      *(uint *)0x764 = uVar4;
      *(uint *)0x766 = uVar2;
      *(uint *)0x768 = uVar4;
      *(undefined1 *)0x74a = 0xff;
    }
    uVar3 = uVar4;
    if (((int)in_DX <= (int)uVar4) && (((int)in_DX < (int)uVar4 || (in_AX <= uVar2)))) {
      local_a = FUN_2281_0008(in_AX,in_DX);
      uVar2 = uVar4 | local_a;
      uVar3 = 0;
      local_8 = uVar4;
      if (uVar2 != 0) {
        uVar3 = FUN_227e_000c();
        if (((int)uVar2 <= *(int *)0x768) &&
           (((int)uVar2 < *(int *)0x768 || (uVar3 < *(uint *)0x766)))) {
          *(uint *)0x766 = uVar3;
          *(uint *)0x768 = uVar2;
        }
        goto LAB_1cc9_02a3;
      }
    }
    uVar4 = FUN_1cc9_0004(((((in_AX >> 1 | (uint)((in_DX & 1) != 0) << 0xf) >> 1 |
                            (uint)(((int)in_DX >> 1 & 1U) != 0) << 0xf) >> 1 |
                           (uint)(((int)in_DX >> 2 & 1U) != 0) << 0xf) >> 1 |
                          (uint)(((int)in_DX >> 3 & 1U) != 0) << 0xf) + 1);
    if (uVar4 == 0) {
      FUN_1cc9_009c();
    }
    else {
      local_a = 0;
      local_8 = uVar4;
    }
    uVar4 = FUN_1d08_0068();
    if (((int)uVar3 <= *(int *)0x760) && (((int)uVar3 < *(int *)0x760 || (uVar4 < *(uint *)0x75e))))
    {
      *(uint *)0x75e = uVar4;
      *(uint *)0x760 = uVar3;
    }
  }
LAB_1cc9_02a3:
  *(bool *)0x74b = local_8 == 0 && local_a == 0;
  if (local_8 != 0 || local_a != 0) {
    FUN_1cc9_010c(local_a,local_8,param_1,param_2);
  }
  FUN_1cc9_00f0(*(undefined2 *)0x76a,*(undefined2 *)0x76c);
  return CONCAT22(local_8,local_a);
}



void __cdecl16far FUN_1cc9_02e2(void)

{
  undefined2 unaff_DS;
  
  FUN_1cc9_0136(0x770,unaff_DS);
  return;
}



void __stdcall16far FUN_1cc9_02ec(char *param_1,undefined2 param_2,int param_3)

{
  char *pcVar1;
  char cVar2;
  int iVar3;
  char *pcVar4;
  char *pcVar5;
  undefined2 uVar6;
  
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  pcVar5 = (char *)param_1;
  pcVar4 = (char *)0x0;
  iVar3 = 8;
  do {
    pcVar1 = pcVar4;
    pcVar4 = pcVar4 + 1;
    cVar2 = *pcVar1;
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 1;
    *pcVar1 = cVar2;
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0 && cVar2 != '\0');
  *pcVar5 = '\0';
  return;
}



undefined2 __stdcall16far FUN_1cc9_0310(undefined2 param_1,uint param_2)

{
  code *pcVar1;
  undefined2 uVar2;
  char cVar3;
  undefined2 unaff_DS;
  
  cVar3 = '\0';
  if (0x9fff < param_2) {
    uVar2 = FUN_2281_004a(param_1,param_2);
  }
  else {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)(param_2,0x9fff < param_2);
    cVar3 = (char)(cVar3 << 7) >> 7;
    uVar2 = CONCAT11(cVar3,cVar3);
  }
  FUN_1cc9_00f0(*(undefined2 *)0x76a,*(undefined2 *)0x76c);
  return uVar2;
}



undefined2 __stdcall16far FUN_1cc9_0360(void)

{
  code *pcVar1;
  uint in_AX;
  char cVar2;
  
  cVar2 = (in_AX >> 1 & 4) != 0;
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  cVar2 = (char)(cVar2 << 7) >> 7;
  return CONCAT11(cVar2,cVar2);
}



void __cdecl16near FUN_1d08_000e(void)

{
  code *pcVar1;
  uint uVar2;
  int in_BX;
  int iVar3;
  int iVar4;
  int iVar5;
  undefined2 unaff_ES;
  
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  iVar3 = *(int *)(in_BX + -2);
  uVar2 = 0;
  iVar4 = 0;
  while( true ) {
    if (*(int *)0x1 == 0) {
      iVar5 = iVar3;
      if (iVar4 != 0) {
        iVar5 = *(int *)0x3;
        *(undefined1 *)0x0 = *(undefined1 *)0x0;
        *(int *)0x3 = *(int *)0x3 + iVar5;
        *(int *)0x3 = *(int *)0x3 + 1;
        iVar5 = iVar4;
      }
      iVar3 = iVar5;
      if (uVar2 < *(uint *)0x3) {
        uVar2 = *(uint *)0x3;
      }
    }
    else {
      iVar5 = 0;
    }
    if (*(char *)0x0 == 'Z') break;
    iVar3 = iVar3 + *(int *)0x3 + 1;
    iVar4 = iVar5;
  }
  return;
}



int __cdecl16far FUN_1d08_0068(void)

{
  int iVar1;
  
  iVar1 = FUN_1d08_000e();
  return iVar1 << 4;
}



uint __cdecl16far FUN_1d08_0082(void)

{
  uint uVar1;
  uint uVar2;
  int in_DX;
  int iVar3;
  
  uVar1 = FUN_1d08_0068();
  iVar3 = in_DX;
  uVar2 = FUN_227e_000c();
  if ((iVar3 <= in_DX) && ((iVar3 < in_DX || (uVar2 < uVar1)))) {
    uVar2 = uVar1;
  }
  return uVar2;
}



void __cdecl16far FUN_1d12_000c(void)

{
  char *in_BX;
  undefined2 unaff_DS;
  byte local_3;
  
  if (*in_BX == '0') {
    local_3 = in_BX[1];
    if ((*(byte *)(local_3 + 0x45a9) & 2) != 0) {
      local_3 = local_3 - 0x20;
    }
    if (local_3 == 0x58) {
      FUN_2050_000a();
      return;
    }
    if (local_3 == 0x42) {
      FUN_2056_000e();
      return;
    }
  }
  thunk_FUN_2388_1e76();
  return;
}



undefined2 __cdecl16far FUN_1d18_0006(void)

{
  undefined2 unaff_DS;
  
  return *(undefined2 *)*(undefined4 *)0x776;
}



undefined2 __cdecl16far FUN_1d18_0022(void)

{
  undefined2 unaff_DS;
  
  return *(undefined2 *)0x50b6;
}



// WARNING: Globals starting with '_' overlap smaller symbols at the same address

void __cdecl16far FUN_1d1c_017d(undefined2 param_1,undefined2 param_2)

{
  code *pcVar1;
  undefined2 in_BX;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x4a8c = 0;
  *(undefined2 *)0x4a7c = 0;
  *(undefined1 *)0x77e = 1;
  *(undefined1 *)0x77f = 1;
  *(undefined1 *)0x780 = 1;
  *(undefined2 *)0x4e72 = 0;
  *(undefined2 *)0x4e74 = 0;
  *(undefined2 *)0x50b6 = 0;
  *(undefined2 *)0x50b8 = 0;
  *(undefined2 *)0x4a6a = 0;
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  _FUN_1000_0008 = in_BX;
  _FUN_1000_000a = unaff_ES;
  (*pcVar1)();
  FUN_205b_0006();
  *(undefined2 *)0x778 = 0x25e7;
  *(undefined2 *)0x776 = 0x4e72;
  *(undefined2 *)0x781 = 0xffff;
  return;
}



void __cdecl16far FUN_1d1c_01f3(void)

{
  code *pcVar1;
  undefined2 unaff_DS;
  undefined2 in_stack_00000002;
  
  if (*(int *)0x781 != 0) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    FUN_205b_0006();
    *(undefined2 *)0x778 = 0x40;
    *(undefined2 *)0x776 = 0x6c;
    unaff_DS = in_stack_00000002;
  }
  *(undefined2 *)0x781 = 0;
  return;
}



void __cdecl16far FUN_1d1c_023d(undefined4 param_1)

{
  uint uVar1;
  undefined2 unaff_DS;
  
  uVar1 = (uint)((ulong)param_1 >> 0x10);
  *(uint *)0x5e26 = (uint)param_1;
  *(uint *)0x5e28 = uVar1;
  *(undefined2 *)0x4a6a = 0;
  *(undefined2 *)0x4c6c = 0;
  *(undefined2 *)0x4a82 = 0;
  *(uint *)0x4ea8 = uVar1 | (uint)param_1;
  return;
}



int __cdecl16far FUN_1d43_000a(void)

{
  long lVar1;
  int iVar2;
  char *pcVar3;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  long lVar4;
  int local_140 [22];
  undefined2 local_114;
  undefined2 local_112;
  char local_76;
  char local_75 [79];
  undefined1 local_26 [20];
  int local_12;
  int local_10;
  uint local_c;
  undefined2 local_a;
  undefined2 local_8;
  
  local_c = 0;
  local_10 = 0;
  local_12 = 0;
  *(undefined2 *)0x74c = 0xf;
  local_140[0] = 0;
  FUN_2388_0626(&local_76);
  iVar2 = FUN_2388_09e8(&local_76,0x2e);
  if (iVar2 == 0) {
    FUN_2388_05e6(&local_76,0x3a8e);
  }
  pcVar3 = &local_76;
  if (local_76 == '*') {
    pcVar3 = local_75;
  }
  FUN_2388_06d6(local_26,pcVar3,8);
  iVar2 = FUN_221a_0000(&local_76,unaff_SS,local_140,unaff_SS);
  lVar1 = (ulong)local_c << 0x10;
  if (iVar2 == 0) {
    local_a = local_114;
    local_8 = local_112;
    lVar4 = FUN_1cc9_0136(local_26,unaff_SS);
    local_c = (uint)((ulong)lVar4 >> 0x10);
    iVar2 = (int)lVar4;
    lVar1 = 0;
    if (lVar4 != 0) {
      lVar4 = FUN_2258_0002(local_140,unaff_SS,1,0,lVar4);
      lVar1 = CONCAT22(local_c,iVar2);
      if (lVar4 != 0) {
        local_10 = local_c;
        local_12 = iVar2;
        lVar1 = CONCAT22(local_c,iVar2);
      }
    }
  }
  local_c = (uint)((ulong)lVar1 >> 0x10);
  if ((lVar1 != 0) && (local_10 == 0 && local_12 == 0)) {
    FUN_1cc9_0310(lVar1);
  }
  if (local_140[0] != 0) {
    FUN_221a_034e(local_140,unaff_SS);
  }
  return local_12;
}



char * __stdcall16far FUN_1d53_0008(int param_1,undefined2 param_2,undefined2 param_3,byte *param_4)

{
  char *pcVar1;
  byte *pbVar2;
  byte bVar3;
  uint in_AX;
  int iVar4;
  int iVar5;
  char *pcVar6;
  undefined1 uVar7;
  uint uVar8;
  char cVar9;
  byte bVar10;
  char cVar11;
  int in_DX;
  char cVar12;
  int *in_BX;
  char *pcVar13;
  byte *pbVar14;
  char *pcVar15;
  char *pcVar16;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined2 uVar17;
  int local_6a;
  uint local_68;
  char *local_66;
  int local_5e;
  char local_5c;
  char local_58 [80];
  undefined2 local_8;
  undefined2 local_6;
  uint local_4;
  
  local_5e = 0;
  local_66 = (char *)0x0;
  local_8 = *(undefined2 *)0x3aaa;
  local_6 = *(undefined2 *)0x3aac;
  FUN_2388_0dec(local_58);
  local_6a = in_DX;
  if (in_DX < 0) {
    local_5e = -in_DX;
    local_6a = 0;
  }
  iVar4 = (uint)*param_4 - local_5e;
  if (iVar4 < 0) {
    iVar4 = 0;
  }
  local_5c = (char)iVar4;
  iVar4 = (int)local_5c;
  iVar5 = iVar4 + local_6a + -1;
  if (*in_BX + -1 < iVar5) {
    iVar5 = (iVar5 - *in_BX) + 1;
    if (iVar4 < iVar5) {
      iVar5 = iVar4;
    }
    local_5c = -((char)iVar5 - local_5c);
  }
  if ('\0' < local_5c) {
    pcVar6 = (char *)FUN_1c91_0000();
    local_4 = in_BX[1];
    pcVar13 = local_58;
    uVar17 = (undefined2)((ulong)param_4 >> 0x10);
    local_66 = pcVar6;
    local_68 = in_AX;
    while( true ) {
      pcVar1 = pcVar13;
      pcVar13 = pcVar13 + 1;
      bVar10 = *pcVar1 - 1;
      if ((char)bVar10 < '\0') break;
      bVar3 = ((byte *)param_4)[bVar10 + 2];
      if (bVar3 == 0) {
        uVar8 = 0;
      }
      else {
        uVar8 = (uint)bVar3;
        local_68 = local_68 + uVar8;
        if (local_4 < local_68) break;
        pbVar14 = *(byte **)((byte *)param_4 + (uint)bVar10 * 2 + 0x82);
        pcVar15 = local_66;
        cVar12 = local_5c;
        if (local_5e != 0) {
          pbVar14 = pbVar14 + local_5e * ((bVar3 - 1 >> 2) + 1);
        }
        while( true ) {
          cVar11 = (char)uVar8;
          uVar8 = CONCAT11(4,cVar11);
          pbVar2 = pbVar14;
          pbVar14 = pbVar14 + 1;
          pcVar16 = pcVar15;
          bVar10 = *pbVar2;
          while( true ) {
            cVar9 = *(char *)((int)&local_8 + (uint)(bVar10 >> 6));
            if (cVar9 != -1) {
              *pcVar16 = cVar9;
            }
            pcVar16 = pcVar16 + 1;
            cVar11 = cVar11 + -1;
            if (cVar11 == '\0') break;
            uVar7 = (undefined1)uVar8;
            cVar9 = (char)(uVar8 >> 8) + -1;
            uVar8 = CONCAT11(cVar9,uVar7);
            bVar10 = bVar10 << 2;
            if (cVar9 == '\0') {
              pbVar2 = pbVar14;
              pbVar14 = pbVar14 + 1;
              bVar10 = *pbVar2;
              uVar8 = CONCAT11(4,uVar7);
            }
          }
          if ((char)(cVar12 + -1) == '\0') break;
          pcVar15 = pcVar15 + local_4;
          cVar12 = cVar12 + -1;
        }
      }
      if ((char)uVar8 != 0) {
        local_66 = local_66 + param_1 + (char)uVar8;
        local_68 = local_68 + param_1;
      }
    }
    local_66 = local_66 + (in_AX - (int)pcVar6);
  }
  return local_66;
}



void __stdcall16far FUN_1d6a_0006(undefined1 param_1)

{
  undefined1 in_AL;
  undefined1 in_DL;
  undefined1 in_BL;
  undefined2 unaff_DS;
  
  *(undefined1 *)0x3aaa = in_AL;
  *(undefined1 *)0x3aab = in_DL;
  *(undefined1 *)0x3aac = in_BL;
  *(undefined1 *)0x3aad = param_1;
  return;
}



int __stdcall16far FUN_1d6c_0002(char *param_1,int param_2,undefined2 param_3)

{
  char *pcVar1;
  char cVar2;
  int in_AX;
  uint uVar3;
  char *pcVar4;
  undefined2 uVar5;
  int local_4;
  
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  pcVar4 = (char *)param_1;
  local_4 = 0;
  cVar2 = *param_1;
  while (cVar2 != '\0') {
    pcVar1 = pcVar4;
    pcVar4 = pcVar4 + 1;
    uVar3 = (uint)*(byte *)(*pcVar1 + param_2 + 1);
    if ((uVar3 != 0) && (*pcVar4 != '\0')) {
      uVar3 = uVar3 + in_AX;
    }
    local_4 = local_4 + uVar3;
    cVar2 = *pcVar4;
  }
  return local_4;
}



void __stdcall16far FUN_1d70_000a(undefined1 *param_1)

{
  undefined1 *puVar1;
  uint uVar2;
  byte bVar3;
  uint uVar4;
  uint uVar5;
  undefined1 *puVar6;
  uint uVar7;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x3ab6 = 1;
  uVar2 = *(uint *)0x3ab4;
  uVar7 = 0x300;
  puVar6 = (undefined1 *)param_1;
  out(0x3c8,0);
  do {
    do {
      bVar3 = in(0x3da);
    } while ((bVar3 & 8) != 0);
    do {
      bVar3 = in(0x3da);
    } while ((bVar3 & 8) == 0);
    uVar5 = uVar7;
    uVar4 = uVar7;
    if (uVar2 < uVar7) {
      uVar5 = uVar2;
      uVar4 = uVar2;
    }
    do {
      puVar1 = puVar6;
      puVar6 = puVar6 + 1;
      out(*puVar1,0x3c9);
      uVar5 = uVar5 - 1;
    } while (uVar5 != 0);
    uVar7 = uVar7 - uVar4;
  } while (uVar7 != 0);
  *(undefined2 *)0x3ab6 = 0;
  return;
}



int __stdcall16far FUN_1d75_000e(undefined4 param_1)

{
  undefined1 *puVar1;
  undefined1 uVar2;
  undefined1 uVar3;
  undefined1 uVar4;
  undefined1 uVar5;
  int in_AX;
  int iVar6;
  int in_DX;
  undefined1 *puVar7;
  undefined2 uVar8;
  
  iVar6 = in_DX * 3;
  uVar8 = (undefined2)((ulong)param_1 >> 0x10);
  puVar7 = (undefined1 *)((int)param_1 + in_AX * 3);
  out(0x43,0);
  uVar2 = in(0x40);
  uVar3 = in(0x40);
  out(0x3c8,(char)in_AX);
  if (*(int *)0x3ab8 == 0) {
    do {
      puVar1 = puVar7;
      puVar7 = puVar7 + 1;
      out(*puVar1,0x3c9);
      iVar6 = iVar6 + -1;
    } while (iVar6 != 0);
  }
  else {
    while (iVar6 != 0) {
      iVar6 = iVar6 + -1;
      puVar1 = puVar7;
      puVar7 = puVar7 + 1;
      out(*puVar1,0x3c9);
    }
  }
  out(0x43,0);
  uVar4 = in(0x40);
  uVar5 = in(0x40);
  return CONCAT11(uVar5,uVar4) - CONCAT11(uVar3,uVar2);
}



void __cdecl16far FUN_1d75_0074(void)

{
  bool bVar1;
  byte bVar2;
  undefined1 uVar3;
  undefined1 uVar4;
  undefined1 uVar5;
  undefined1 uVar6;
  int iVar7;
  uint uVar8;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  uint local_30c;
  int local_306;
  int local_304;
  undefined1 local_302 [768];
  
  *(undefined2 *)0x3ab8 = 0;
  uVar8 = 0;
  iVar7 = 0x20;
  do {
    do {
      bVar2 = in(0x3da);
    } while ((bVar2 & 8) != 0);
    do {
      bVar2 = in(0x3da);
    } while ((bVar2 & 8) == 0);
    out(0x43,0);
    uVar3 = in(0x40);
    uVar4 = in(0x40);
    do {
      bVar2 = in(0x3da);
    } while ((bVar2 & 8) != 0);
    out(0x43,0);
    uVar5 = in(0x40);
    uVar6 = in(0x40);
    uVar8 = uVar8 + (CONCAT11(uVar6,uVar5) - CONCAT11(uVar4,uVar3));
    iVar7 = iVar7 + -1;
  } while (iVar7 != 0);
  *(uint *)0x3ab0 = uVar8 >> 5;
  FUN_205c_000a(local_302,unaff_SS);
  local_30c = 0x40;
  local_304 = 0x80;
  bVar1 = false;
  for (local_306 = 0; ((2 < local_30c || (!bVar1)) && (local_306 < 0x20)); local_306 = local_306 + 1
      ) {
    uVar8 = FUN_1d75_000e(local_302,unaff_SS);
    if (*(uint *)0x3ab0 < uVar8) {
      local_304 = local_304 - local_30c;
      if (local_304 < 1) {
        local_304 = 1;
      }
      bVar1 = false;
    }
    else {
      local_304 = local_304 + local_30c;
      if (0x100 < local_304) {
        local_304 = 0x100;
      }
      bVar1 = true;
    }
    if (2 < local_30c) {
      local_30c = (int)local_30c >> 1;
    }
  }
  if (bVar1) {
    *(int *)0x3ab2 = local_304 * 0xe;
  }
  else {
    *(undefined2 *)0x3ab2 = 0x20;
  }
  *(int *)0x3ab4 = *(int *)0x3ab2 * 3;
  *(undefined2 *)0x3aae = 1;
  return;
}



void __stdcall16far FUN_1d8f_0000(int param_1,int param_2,undefined2 param_3)

{
  char *pcVar1;
  char *pcVar2;
  char cVar3;
  undefined2 uVar4;
  int iVar5;
  uint in_AX;
  char cVar6;
  int iVar7;
  int in_DX;
  int iVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int *in_BX;
  int iVar12;
  int iVar13;
  int iVar14;
  char *pcVar15;
  char *pcVar16;
  char *pcVar17;
  char *pcVar18;
  undefined2 unaff_DS;
  int local_2c;
  int local_24;
  int local_20;
  int local_6;
  
  iVar8 = 1;
  if ((int)in_AX < 0) {
    iVar8 = -1;
  }
  param_2 = (in_AX & 0x7fff) * 0xc + param_2;
  pcVar18 = (char *)*(undefined2 *)(param_2 + 0x36);
  uVar4 = *(undefined2 *)(param_2 + 0x38);
  iVar14 = in_BX[2];
  iVar11 = in_BX[3];
  iVar5 = in_BX[1];
  iVar7 = *(int *)(param_2 + 0x3e);
  iVar13 = *(int *)(param_2 + 0x40);
  local_24 = 0;
  iVar9 = in_DX + iVar7 + -1;
  iVar12 = iVar7;
  if (in_DX < 0) {
    iVar12 = iVar7 + in_DX;
    local_24 = -in_DX;
  }
  iVar10 = iVar9 - (in_BX[1] + -1);
  if (iVar10 != 0 && in_BX[1] + -1 <= iVar9) {
    iVar12 = iVar12 - iVar10;
  }
  local_20 = local_24 + iVar12;
  if (0 < iVar12) {
    local_2c = in_DX;
    if (iVar8 != 1) {
      local_2c = in_DX + iVar7 + -1;
      local_24 = -(iVar7 - local_20);
      local_20 = (iVar7 - local_20) + iVar12;
    }
    iVar7 = 0;
    iVar12 = param_1 + iVar13 + -1;
    if (param_1 < 0) {
      iVar13 = iVar13 + param_1;
      iVar7 = -param_1;
    }
    iVar9 = iVar12 - (*in_BX + -1);
    if (iVar9 != 0 && *in_BX + -1 <= iVar12) {
      iVar13 = iVar13 - iVar9;
    }
    if (0 < iVar13) {
      for (param_1 = iVar7 + param_1; param_1 != 0; param_1 = param_1 + -1) {
        iVar14 = iVar14 + iVar5;
        if (iVar14 < 0) {
          iVar14 = iVar14 + -0x7000;
          iVar11 = iVar11 + 0x700;
        }
      }
      pcVar15 = (char *)(iVar14 + local_2c + local_24);
      local_24 = local_24 * iVar8;
      iVar14 = -1;
LAB_1d8f_0127:
      local_6 = 0;
      iVar14 = iVar14 + 1;
      if (iVar14 < iVar7 + iVar13) {
        if (iVar7 <= iVar14) {
          iVar12 = 0;
          pcVar1 = pcVar18;
          pcVar18 = pcVar18 + 1;
          if (*pcVar1 == -1) {
            local_6 = -1;
          }
          else {
            pcVar16 = pcVar15;
            if (*pcVar1 == -3) {
              while (iVar12 < local_20) {
                pcVar17 = pcVar18 + 1;
                cVar6 = *pcVar18;
                if (cVar6 == -1) goto LAB_1d8f_0161;
                pcVar18 = pcVar18 + 2;
                cVar3 = *pcVar17;
                do {
                  if ((local_24 <= iVar12) && (iVar12 < local_20)) {
                    if (cVar3 != -3) {
                      *pcVar16 = cVar3;
                    }
                    pcVar16 = pcVar16 + iVar8;
                  }
                  iVar12 = iVar12 + 1;
                  cVar6 = cVar6 + -1;
                } while (cVar6 != '\0');
              }
            }
            else {
              while (iVar12 < local_20) {
                pcVar17 = pcVar18 + 1;
                cVar6 = *pcVar18;
                if (cVar6 == -1) goto LAB_1d8f_0161;
                if (cVar6 == -2) {
                  pcVar2 = pcVar18 + 2;
                  cVar6 = *pcVar17;
                  pcVar18 = pcVar18 + 3;
                  cVar3 = *pcVar2;
                  do {
                    if ((local_24 <= iVar12) && (iVar12 < local_20)) {
                      if (cVar3 != -3) {
                        *pcVar16 = cVar3;
                      }
                      pcVar16 = pcVar16 + iVar8;
                    }
                    iVar12 = iVar12 + 1;
                    cVar6 = cVar6 + -1;
                  } while (cVar6 != '\0');
                }
                else {
                  if ((local_24 <= iVar12) && (iVar12 < local_20)) {
                    if (cVar6 != -3) {
                      *pcVar16 = cVar6;
                    }
                    pcVar16 = pcVar16 + iVar8;
                  }
                  iVar12 = iVar12 + 1;
                  pcVar18 = pcVar17;
                }
              }
            }
          }
          goto LAB_1d8f_01d2;
        }
        goto LAB_1d8f_01e4;
      }
    }
  }
  return;
LAB_1d8f_0161:
  local_6 = -1;
  pcVar18 = pcVar17;
LAB_1d8f_01d2:
  pcVar15 = pcVar15 + iVar5;
  if ((int)pcVar15 < 0) {
    pcVar15 = pcVar15 + -0x7000;
    iVar11 = iVar11 + 0x700;
  }
LAB_1d8f_01e4:
  if (local_6 != -1) {
    do {
      pcVar1 = pcVar18;
      pcVar18 = pcVar18 + 1;
    } while (*pcVar1 != -1);
  }
  goto LAB_1d8f_0127;
}



void __stdcall16far FUN_1dae_000a(int param_1,int param_2,int param_3,undefined2 param_4)

{
  char *pcVar1;
  char *pcVar2;
  char cVar3;
  undefined2 uVar4;
  int iVar5;
  uint in_AX;
  int iVar6;
  char cVar7;
  int iVar8;
  int iVar9;
  int in_DX;
  int iVar10;
  int iVar11;
  int iVar12;
  int *in_BX;
  int iVar13;
  char *pcVar14;
  char *pcVar15;
  char *pcVar16;
  char *pcVar17;
  int iVar18;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int local_172;
  int local_16a;
  int local_162;
  int local_156;
  char local_154 [320];
  uint local_14;
  int local_12;
  undefined2 *local_10;
  undefined2 local_e;
  int local_c;
  uint local_8;
  int local_6;
  int local_4;
  
  iVar8 = in_BX[1];
  iVar12 = *in_BX;
  local_12 = 1;
  if ((int)in_AX < 0) {
    local_12 = -1;
  }
  param_3 = (in_AX & 0x7fff) * 0xc + param_3;
  local_10 = (undefined2 *)(param_3 + 0x36);
  local_e = param_4;
  pcVar17 = (char *)*(undefined2 *)(param_3 + 0x36);
  uVar4 = *(undefined2 *)(param_3 + 0x38);
  iVar13 = in_BX[2];
  iVar11 = in_BX[3];
  local_4 = in_BX[1];
  iVar9 = *(int *)(param_3 + 0x3e);
  iVar5 = *(int *)(param_3 + 0x40);
  iVar10 = iVar9;
  if (iVar9 < iVar5) {
    iVar10 = iVar5;
  }
  iVar18 = 0;
  local_14 = 0;
  local_156 = 0;
  iVar6 = 0x32;
  do {
    iVar6 = iVar6 + param_1;
    if (iVar6 < 100) {
      local_154[iVar18] = '\0';
    }
    else {
      local_154[iVar18] = -1;
      iVar6 = iVar6 + -100;
      if (iVar18 < iVar9) {
        local_14 = local_14 + 1;
      }
      if (iVar18 < iVar5) {
        local_156 = local_156 + 1;
      }
    }
    iVar18 = iVar18 + 1;
  } while (iVar18 < iVar10);
  local_172 = in_DX - (local_14 >> 1);
  iVar9 = (param_2 - local_156) + 1;
  local_16a = 0;
  iVar10 = local_172 + local_14 + -1;
  local_8 = local_14;
  if (local_172 < 0) {
    local_8 = local_14 + local_172;
    local_16a = -local_172;
  }
  iVar6 = iVar10 - (iVar8 + -1);
  if (iVar6 != 0 && iVar8 + -1 <= iVar10) {
    local_8 = local_8 - iVar6;
  }
  local_162 = local_16a + local_8;
  if (0 < (int)local_8) {
    if (local_12 != 1) {
      local_172 = local_172 + local_14 + -1;
      local_16a = -(local_14 - local_162);
      local_162 = (local_14 - local_162) + local_8;
    }
    iVar8 = 0;
    iVar10 = iVar9 + local_156 + -1;
    if (iVar9 < 0) {
      local_156 = local_156 + iVar9;
      iVar8 = -iVar9;
    }
    iVar6 = iVar10 - (iVar12 + -1);
    if (iVar6 != 0 && iVar12 + -1 <= iVar10) {
      local_156 = local_156 - iVar6;
    }
    local_c = local_156;
    if (0 < local_156) {
      for (iVar9 = iVar8 + iVar9; iVar9 != 0; iVar9 = iVar9 + -1) {
        iVar13 = iVar13 + local_4;
        if (iVar13 < 0) {
          iVar13 = iVar13 + -0x7000;
          iVar11 = iVar11 + 0x700;
        }
      }
      pcVar14 = (char *)(iVar13 + local_172 + local_16a);
      local_16a = local_16a * local_12;
      iVar13 = -1;
      iVar12 = -1;
LAB_1dae_01b4:
      local_6 = 0;
      iVar9 = iVar12 + 1;
      if (iVar9 < iVar5) {
        if (local_154[iVar12 + 1] == '\0') goto LAB_1dae_02c1;
        iVar13 = iVar13 + 1;
        if (iVar13 < iVar8 + local_156) {
          if (iVar13 < iVar8) goto LAB_1dae_02c1;
          iVar10 = 0;
          iVar12 = 0;
          pcVar1 = pcVar17;
          pcVar17 = pcVar17 + 1;
          if (*pcVar1 == -1) {
            local_6 = -1;
          }
          else {
            pcVar15 = pcVar14;
            if (*pcVar1 == -3) {
              while (iVar12 < local_162) {
                pcVar16 = pcVar17 + 1;
                cVar7 = *pcVar17;
                if (cVar7 == -1) goto LAB_1dae_020d;
                pcVar17 = pcVar17 + 2;
                cVar3 = *pcVar16;
                do {
                  if (local_154[iVar10] != '\0') {
                    if ((local_16a <= iVar12) && (iVar12 < local_162)) {
                      if (cVar3 != -3) {
                        *pcVar15 = cVar3;
                      }
                      pcVar15 = pcVar15 + local_12;
                    }
                    iVar12 = iVar12 + 1;
                  }
                  iVar10 = iVar10 + 1;
                  cVar7 = cVar7 + -1;
                } while (cVar7 != '\0');
              }
            }
            else {
              while (iVar12 < local_162) {
                pcVar16 = pcVar17 + 1;
                cVar7 = *pcVar17;
                if (cVar7 == -1) goto LAB_1dae_020d;
                if (cVar7 == -2) {
                  pcVar2 = pcVar17 + 2;
                  cVar7 = *pcVar16;
                  pcVar17 = pcVar17 + 3;
                  cVar3 = *pcVar2;
                  do {
                    if (local_154[iVar10] != '\0') {
                      if ((local_16a <= iVar12) && (iVar12 < local_162)) {
                        if (cVar3 != -3) {
                          *pcVar15 = cVar3;
                        }
                        pcVar15 = pcVar15 + local_12;
                      }
                      iVar12 = iVar12 + 1;
                    }
                    iVar10 = iVar10 + 1;
                    cVar7 = cVar7 + -1;
                  } while (cVar7 != '\0');
                }
                else {
                  if (local_154[iVar10] != '\0') {
                    if ((local_16a <= iVar12) && (iVar12 < local_162)) {
                      if (cVar7 != -3) {
                        *pcVar15 = cVar7;
                      }
                      pcVar15 = pcVar15 + local_12;
                    }
                    iVar12 = iVar12 + 1;
                  }
                  iVar10 = iVar10 + 1;
                  pcVar17 = pcVar16;
                }
              }
            }
          }
          goto LAB_1dae_02ae;
        }
      }
    }
  }
  return;
LAB_1dae_020d:
  local_6 = -1;
  pcVar17 = pcVar16;
LAB_1dae_02ae:
  pcVar14 = pcVar14 + local_4;
  if ((int)pcVar14 < 0) {
    pcVar14 = pcVar14 + -0x7000;
    iVar11 = iVar11 + 0x700;
  }
LAB_1dae_02c1:
  iVar12 = iVar9;
  if (local_6 != -1) {
    do {
      pcVar1 = pcVar17;
      pcVar17 = pcVar17 + 1;
    } while (*pcVar1 != -1);
  }
  goto LAB_1dae_01b4;
}



undefined2 * __cdecl16far FUN_1ddb_0008(void)

{
  int *piVar1;
  int *piVar2;
  undefined4 uVar3;
  uint in_AX;
  int iVar4;
  uint uVar5;
  undefined2 *puVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  undefined2 uVar11;
  undefined2 uVar12;
  undefined2 uVar13;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar14;
  long lVar15;
  undefined4 uVar16;
  uint local_22c;
  byte *local_224;
  uint local_222;
  int local_220;
  undefined2 *local_21e;
  int local_21c;
  uint local_214;
  int local_212;
  undefined4 local_210;
  undefined1 local_20c [20];
  char *local_1f8;
  int local_1f6 [2];
  char local_1f2;
  undefined2 local_1f0;
  undefined2 local_1ee;
  undefined2 local_1ec;
  undefined2 local_1ea;
  undefined2 local_1e8;
  undefined2 local_1e6;
  undefined2 local_1e4;
  int local_1de;
  uint auStack_1c6 [77];
  uint local_12c;
  int local_12a;
  undefined4 local_128;
  uint local_124;
  int local_122;
  char local_120;
  undefined1 local_11f;
  int local_11e;
  int local_11c;
  int local_11a [16];
  int local_fa;
  undefined1 local_f8 [104];
  undefined2 local_90;
  undefined2 local_8e;
  uint local_8c;
  int local_8a;
  undefined4 local_78;
  uint local_74;
  int local_72;
  undefined4 local_70;
  undefined4 local_6c;
  undefined4 local_68;
  int *local_64;
  undefined2 local_62;
  uint local_60;
  int local_5e;
  int local_5c;
  undefined2 uStack_5a;
  int local_58;
  char local_56;
  char local_55 [79];
  int local_6;
  int local_4;
  
  local_6c = 0;
  local_68 = (undefined2 *)0x0;
  local_21c = 0;
  local_21e = (undefined2 *)0x0;
  local_128 = (int *)0x0;
  *(undefined2 *)0x74c = 0xd;
  local_1f6[0] = 0;
  FUN_2388_0626(&local_56);
  iVar4 = FUN_2388_09e8(&local_56,0x2e);
  if (iVar4 == 0) {
    FUN_2388_05e6(&local_56,0x3aba);
  }
  FUN_2388_0626(local_20c,0x3abe);
  local_1f8 = &local_56;
  FUN_2388_0a88(local_1f8);
  if (local_56 == '*') {
    local_1f8 = local_55;
  }
  if ((*local_1f8 == 'R') && (local_1f8[1] == 'M')) {
    local_1f8 = local_1f8 + 2;
  }
  FUN_2388_06a0(local_20c,local_1f8,6);
  iVar4 = FUN_221a_0000(&local_56,unaff_SS,local_1f6,unaff_SS);
  if (iVar4 != 0) {
    *(undefined2 *)0x3ade = 0xffff;
    goto LAB_1ddb_08f9;
  }
  *(undefined2 *)0x3ade = 0xfffe;
  lVar15 = FUN_2258_0002(local_1f6,unaff_SS,1,0,&local_120,unaff_SS);
  if (lVar15 == 0) goto LAB_1ddb_08f9;
  local_22c = in_AX;
  if (local_11a[2] != 0) {
    local_22c = in_AX | 4;
  }
  local_6 = local_fa << 4;
  local_74 = local_fa * 0xc + 0x42;
  local_72 = (int)local_74 >> 0xf;
  if ((local_22c & 4) != 0) {
    bVar14 = 0xff97 < local_74;
    local_74 = local_fa * 0xc + 0xaa;
    local_72 = local_72 + (uint)bVar14;
  }
  local_124 = local_74;
  local_122 = local_72;
  if (local_120 != '\0') {
    uVar5 = (local_fa + 0x22) * 8;
    local_124 = local_74 + uVar5;
    local_122 = local_72 + (uint)CARRY2(local_74,uVar5);
  }
  local_222 = local_124;
  local_220 = local_122;
  if (((local_22c & 2) == 0) && (local_120 == '\0')) {
    local_222 = local_124 + local_8c;
    local_220 = local_122 + local_8a + (uint)CARRY2(local_124,local_8c);
  }
  if (*(int *)0x3ae6 != 0 || *(int *)0x3ae4 != 0) {
    if ((local_220 <= *(int *)0x5ec4) &&
       ((local_220 < *(int *)0x5ec4 || (local_222 <= *(uint *)0x5ec2)))) {
      local_68 = (undefined2 *)CONCAT22(*(undefined2 *)0x3ae6,(undefined2 *)*(undefined2 *)0x3ae4);
    }
  }
  *(uint *)0x5a9a = local_222;
  *(int *)0x5a9c = local_220;
  if (local_68._2_2_ == 0 && (undefined2 *)local_68 == (undefined2 *)0x0) {
    local_68 = (undefined2 *)FUN_1cc9_0136(local_20c,unaff_SS);
  }
  if ((local_68._2_2_ == 0 && (undefined2 *)local_68 == (undefined2 *)0x0) ||
     (local_6c = FUN_1cc9_0136(0x3ac4,unaff_DS), local_6c == 0)) {
LAB_1ddb_01f0:
    *(undefined2 *)0x3ade = 0xfffc;
    goto LAB_1ddb_08f9;
  }
  uVar11 = (undefined2)((ulong)local_68 >> 0x10);
  puVar6 = (undefined2 *)local_68;
  puVar6[0x20] = 0;
  puVar6[0x1f] = 0;
  puVar6[0x1c] = 0;
  puVar6[0x1b] = 0;
  puVar6[0x18] = 0;
  puVar6[0x17] = 0;
  puVar6[0x1a] = 0;
  puVar6[0x19] = 0;
  puVar6[0x1e] = 0;
  puVar6[0x1d] = 0;
  lVar15 = FUN_2258_0002(local_1f6,unaff_SS,1,0,local_6c);
  if (lVar15 == 0) {
    *(undefined2 *)0x3ade = 0xfffe;
    goto LAB_1ddb_08f9;
  }
  if (local_11a[3] == 0) {
    local_128 = (int *)FUN_1cc9_0136(0x3acd,unaff_DS);
    piVar1 = local_128;
    if (local_128 == (int *)0x0) goto LAB_1ddb_01f0;
LAB_1ddb_02b7:
    lVar15 = FUN_2258_0002(local_1f6,unaff_SS,1,0,piVar1);
    if (lVar15 == 0) goto LAB_1ddb_08f9;
  }
  else {
    if (*(int *)0x3ae2 != 0 || *(int *)0x3ae0 != 0) {
      local_64 = (int *)*(undefined2 *)0x3ae0;
      local_62 = *(undefined2 *)0x3ae2;
      local_128 = (int *)0x0;
      piVar1 = (int *)CONCAT22(local_62,local_64);
      goto LAB_1ddb_02b7;
    }
    FUN_2388_0762(local_1f0,&local_60);
    local_58 = local_1de;
    iVar4 = local_1de * 5;
    iVar8 = local_1de * 5;
    local_1de = local_1de + 1;
    FUN_2388_07fe(local_1f0,auStack_1c6[iVar4] + local_60,
                  auStack_1c6[iVar8 + 1] + local_5e + (uint)CARRY2(auStack_1c6[iVar4],local_60),0);
  }
  uVar11 = (undefined2)((ulong)local_68 >> 0x10);
  puVar6 = (undefined2 *)local_68;
  *(char *)(puVar6 + 0x16) = local_120;
  if ((local_11e == 0) || (3 < local_11c)) {
    *local_68 = 0;
  }
  else {
    *local_68 = 1;
  }
  puVar6[1] = local_11c;
  puVar6[2] = local_fa;
  puVar6[0x14] = local_90;
  puVar6[0x15] = local_8e;
  iVar4 = 0;
  do {
    puVar6[iVar4 + 4] = local_11a[iVar4];
    iVar4 = iVar4 + 1;
  } while (iVar4 < 0x10);
  if ((local_22c & 4) != 0) {
    iVar4 = (int)puVar6 + (local_74 - 0x68);
    puVar6[0x1f] = iVar4;
    puVar6[0x20] = uVar11;
    uVar16 = FUN_204f_0002(iVar4,uVar11);
    uVar11 = (undefined2)((ulong)local_68 >> 0x10);
    puVar6 = (undefined2 *)local_68;
    puVar6[0x1f] = (int)uVar16;
    puVar6[0x20] = (int)((ulong)uVar16 >> 0x10);
    FUN_2388_0c4a(puVar6[0x1f],puVar6[0x20],local_f8);
  }
  local_210 = FUN_204f_0002(local_124 + (int)(undefined2 *)local_68,local_68._2_2_);
  iVar4 = 0;
  uVar16 = local_210;
  while( true ) {
    local_78._2_2_ = (undefined2)((ulong)uVar16 >> 0x10);
    local_78._0_2_ = (int)uVar16;
    uVar11 = (undefined2)((ulong)local_68 >> 0x10);
    puVar6 = (undefined2 *)local_68;
    if ((int)puVar6[2] <= iVar4) break;
    iVar8 = iVar4 * 0x10 + (int)local_6c;
    puVar6[iVar4 * 6 + 0x23] = *(undefined2 *)(iVar8 + 8);
    puVar6[iVar4 * 6 + 0x24] = *(undefined2 *)(iVar8 + 10);
    puVar6[iVar4 * 6 + 0x25] = *(undefined2 *)(iVar8 + 0xc);
    puVar6[iVar4 * 6 + 0x26] = *(undefined2 *)(iVar8 + 0xe);
    if (((local_22c & 2) == 0) && (local_120 == '\0')) {
      puVar6[iVar4 * 6 + 0x21] = (int)local_78;
      puVar6[iVar4 * 6 + 0x22] = local_78._2_2_;
      uVar3 = uVar16;
      uVar16 = FUN_204f_0002((int)local_78 + *(int *)(iVar8 + 4),local_78._2_2_);
      local_78 = uVar3;
    }
    else {
      puVar6[iVar4 * 6 + 0x22] = 0;
      puVar6[iVar4 * 6 + 0x21] = 0;
    }
    iVar4 = iVar4 + 1;
  }
  local_78 = uVar16;
  if (((local_22c & 2) == 0) && (local_120 == '\0')) {
    lVar15 = FUN_2258_0002(local_1f6,unaff_SS,1,0,local_210);
    if (lVar15 == 0) goto LAB_1ddb_08f9;
  }
  if (local_11a[3] == 0) {
    if ((local_22c & 9) == 0) {
      iVar4 = FUN_2074_0416(*(undefined2 *)0x4404,*(undefined2 *)0x4406,(int *)local_128,
                            local_128._2_2_);
      uVar12 = (undefined2)((ulong)local_68 >> 0x10);
      ((undefined2 *)local_68)[3] = iVar4;
      if (iVar4 < 0) {
        *(undefined2 *)0x3ade = 0xfff7;
        goto LAB_1ddb_08f9;
      }
      if (((local_22c & 2) == 0) && (local_120 == '\0')) {
        uVar11 = local_128._2_2_;
        goto LAB_1ddb_0664;
      }
    }
    else {
      ((undefined2 *)local_68)[3] = 0;
      if ((local_22c & 8) != 0) {
        iVar4 = 0;
        local_224 = (byte *)0x0;
        while( true ) {
          uVar11 = (undefined2)((ulong)local_128 >> 0x10);
          if (*local_128 <= iVar4) break;
          bVar14 = false;
          local_4 = 0;
          while( true ) {
            piVar2 = (int *)local_128;
            uVar11 = local_128._2_2_;
            if ((bVar14) || (3 < local_4)) break;
            iVar8 = FUN_2388_0bee((int *)local_128 + iVar4 * 3 + 1,local_128._2_2_,
                                  local_4 * 3 + 0x5b20);
            if (iVar8 == 0) {
              bVar14 = true;
              *(undefined1 *)((int)piVar2 + iVar4 * 6 + 5) = (undefined1)local_4;
            }
            local_4 = local_4 + 1;
          }
          if (!bVar14) {
            FUN_2388_0c4a((uint)*local_224 * 3 + 0x5b20);
            *(byte *)((int)piVar2 + iVar4 * 6 + 5) = *local_224;
            local_224 = local_224 + 1;
            if (6 < (int)local_224) {
              local_224 = (byte *)0x6;
            }
          }
          iVar4 = iVar4 + 1;
        }
        uVar12 = local_68._2_2_;
LAB_1ddb_0664:
        FUN_2069_0006((int *)local_128,uVar11,(undefined2 *)local_68,uVar12);
      }
    }
  }
  if (local_120 != '\0') {
    iVar4 = local_74 + (int)(undefined2 *)local_68;
    ((undefined2 *)local_68)[0x1b] = iVar4;
    ((undefined2 *)local_68)[0x1c] = local_68._2_2_;
    ((undefined2 *)local_68)[0x17] = (char *)(iVar4 + 0xfc);
    ((undefined2 *)local_68)[0x18] = local_68._2_2_;
    local_70 = (char *)CONCAT22(local_68._2_2_,(char *)(iVar4 + 0xfc));
    iVar8 = iVar4 + 0x110;
    ((undefined2 *)local_68)[0x19] = iVar8;
    ((undefined2 *)local_68)[0x1a] = local_68._2_2_;
    _local_5c = CONCAT22(local_68._2_2_,iVar8);
    iVar7 = 0;
    while( true ) {
      if (*local_128 <= iVar7) break;
      *(undefined1 *)((int)*(undefined4 *)((undefined2 *)local_68 + 0x1b) + iVar7) =
           *(undefined1 *)((int)(int *)local_128 + iVar7 * 6 + 5);
      iVar7 = iVar7 + 1;
    }
    *(undefined1 *)(iVar4 + 0xfd) = local_11f;
    *local_70 = local_1f2;
    if ((local_1f2 == '\x01') || (local_1f2 == '\x02')) {
      *(undefined2 *)(iVar4 + 0x104) = local_1ee;
      *(undefined2 *)(iVar4 + 0x106) = local_1e6;
      *(undefined2 *)(iVar4 + 0x108) = local_1e4;
      *(undefined2 *)(iVar4 + 0x10a) = local_1ec;
      *(undefined2 *)(iVar4 + 0x10c) = local_1ea;
      *(undefined2 *)(iVar4 + 0x10e) = local_1e8;
      local_212 = 0;
      local_214 = 0;
      local_12a = 0;
      local_12c = 0;
      for (iVar4 = 0; iVar4 < (int)((undefined2 *)local_68)[2]; iVar4 = iVar4 + 1) {
        iVar9 = iVar4 * 8;
        *(uint *)(iVar9 + iVar8) = local_12c;
        *(int *)(iVar9 + iVar8 + 2) = local_12a;
        uVar11 = (undefined2)((ulong)local_6c >> 0x10);
        uVar5 = *(uint *)((int)local_6c + iVar4 * 0x10 + 4);
        iVar7 = *(int *)((int)local_6c + iVar4 * 0x10 + 6);
        iVar9 = iVar8 + iVar9;
        *(uint *)(iVar9 + 4) = uVar5;
        *(int *)(iVar9 + 6) = iVar7;
        bVar14 = CARRY2(local_12c,uVar5);
        local_12c = local_12c + uVar5;
        local_12a = local_12a + iVar7 + (uint)bVar14;
        uVar5 = *(uint *)(iVar9 + 4);
        iVar7 = *(int *)(iVar9 + 6);
        if ((iVar7 <= local_212) && ((iVar7 < local_212 || (uVar5 < local_214)))) {
          uVar5 = local_214;
          iVar7 = local_212;
        }
        local_214 = uVar5;
        local_212 = iVar7;
      }
    }
    else {
      *(undefined2 *)(iVar4 + 0xfe) = local_1f0;
      FUN_2388_0762(local_1f0,&local_12c);
      uVar11 = (undefined2)((ulong)local_70 >> 0x10);
      *(uint *)((char *)local_70 + 4) = local_12c;
      *(int *)((char *)local_70 + 6) = local_12a;
      local_212 = 0;
      local_214 = 0;
      for (iVar4 = 0; iVar4 < (int)((undefined2 *)local_68)[2]; iVar4 = iVar4 + 1) {
        iVar8 = iVar4 * 0x10;
        uVar12 = (undefined2)((ulong)local_6c >> 0x10);
        iVar9 = (int)local_6c;
        uVar11 = *(undefined2 *)(iVar8 + iVar9 + 2);
        iVar7 = iVar4 * 8;
        uVar13 = (undefined2)((ulong)_local_5c >> 0x10);
        iVar10 = (int)_local_5c;
        *(undefined2 *)(iVar7 + iVar10) = *(undefined2 *)(iVar8 + iVar9);
        *(undefined2 *)(iVar7 + iVar10 + 2) = uVar11;
        uVar5 = *(uint *)(iVar9 + iVar8 + 4);
        iVar8 = *(int *)(iVar9 + iVar8 + 6);
        *(uint *)(iVar10 + iVar7 + 4) = uVar5;
        *(int *)(iVar10 + iVar7 + 6) = iVar8;
        if ((iVar8 <= local_212) && ((iVar8 < local_212 || (uVar5 < local_214)))) {
          uVar5 = local_214;
          iVar8 = local_212;
        }
        local_214 = uVar5;
        local_212 = iVar8;
      }
    }
    if ((local_22c & 2) == 0) {
      if (local_128._2_2_ != 0 || (int *)local_128 != (int *)0x0) {
        FUN_1cc9_0310((int *)local_128,local_128._2_2_);
      }
      local_128 = (int *)0x0;
      if (local_6c._2_2_ != 0 || (int)local_6c != 0) {
        FUN_1cc9_0310((int)local_6c,local_6c._2_2_);
      }
      local_6c = 0;
      uVar16 = FUN_1cc9_0136(0x3ad5,unaff_DS);
      iVar4 = (int)((ulong)uVar16 >> 0x10);
      uVar11 = (undefined2)((ulong)local_68 >> 0x10);
      puVar6 = (undefined2 *)local_68;
      puVar6[0x1d] = (int)uVar16;
      puVar6[0x1e] = iVar4;
      if (iVar4 == 0 && puVar6[0x1d] == 0) goto LAB_1ddb_08f9;
      FUN_2388_0c4a(puVar6 + 4,uVar11,&local_214);
      local_1f6[0] = 0;
    }
    else {
      uVar11 = (undefined2)((ulong)local_68 >> 0x10);
      ((undefined2 *)local_68)[0x1e] = 0;
      ((undefined2 *)local_68)[0x1d] = 0;
    }
  }
  local_21e = (undefined2 *)local_68;
  local_21c = local_68._2_2_;
LAB_1ddb_08f9:
  if (local_1f6[0] != 0) {
    FUN_221a_034e(local_1f6,unaff_SS);
  }
  if (local_128._2_2_ != 0 || (int *)local_128 != (int *)0x0) {
    FUN_1cc9_0310((int *)local_128,local_128._2_2_);
  }
  if (local_6c._2_2_ != 0 || (int)local_6c != 0) {
    FUN_1cc9_0310((int)local_6c,local_6c._2_2_);
  }
  if ((local_68._2_2_ != 0 || (undefined2 *)local_68 != (undefined2 *)0x0) &&
     ((((undefined2 *)*(undefined2 *)0x3ae4 != (undefined2 *)local_68 ||
       (*(int *)0x3ae6 != local_68._2_2_)) && (local_21c == 0 && local_21e == (undefined2 *)0x0))))
  {
    FUN_1cc9_0310((undefined2 *)local_68,local_68._2_2_);
  }
  return local_21e;
}



void __stdcall16far FUN_1e71_000c(int param_1,int param_2,undefined2 param_3)

{
  char *pcVar1;
  char *pcVar2;
  char cVar3;
  undefined2 uVar4;
  int iVar5;
  uint in_AX;
  char cVar6;
  int iVar7;
  int in_DX;
  int iVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int *in_BX;
  int iVar12;
  int iVar13;
  int iVar14;
  char *pcVar15;
  char *pcVar16;
  char *pcVar17;
  char *pcVar18;
  undefined2 unaff_DS;
  int local_2c;
  int local_24;
  int local_20;
  int local_6;
  
  iVar8 = 1;
  if ((int)in_AX < 0) {
    iVar8 = -1;
  }
  param_2 = (in_AX & 0x7fff) * 0xc + param_2;
  pcVar18 = (char *)*(undefined2 *)(param_2 + 0x36);
  uVar4 = *(undefined2 *)(param_2 + 0x38);
  iVar14 = in_BX[2];
  iVar11 = in_BX[3];
  iVar5 = in_BX[1];
  iVar7 = *(int *)(param_2 + 0x3e);
  iVar13 = *(int *)(param_2 + 0x40);
  local_24 = 0;
  iVar9 = in_DX + iVar7 + -1;
  iVar12 = iVar7;
  if (in_DX < 0) {
    iVar12 = iVar7 + in_DX;
    local_24 = -in_DX;
  }
  iVar10 = iVar9 - (in_BX[1] + -1);
  if (iVar10 != 0 && in_BX[1] + -1 <= iVar9) {
    iVar12 = iVar12 - iVar10;
  }
  local_20 = local_24 + iVar12;
  if (0 < iVar12) {
    local_2c = in_DX;
    if (iVar8 != 1) {
      local_2c = in_DX + iVar7 + -1;
      local_24 = -(iVar7 - local_20);
      local_20 = (iVar7 - local_20) + iVar12;
    }
    iVar7 = 0;
    iVar12 = param_1 + iVar13 + -1;
    if (param_1 < 0) {
      iVar13 = iVar13 + param_1;
      iVar7 = -param_1;
    }
    iVar9 = iVar12 - (*in_BX + -1);
    if (iVar9 != 0 && *in_BX + -1 <= iVar12) {
      iVar13 = iVar13 - iVar9;
    }
    if (0 < iVar13) {
      for (param_1 = iVar7 + param_1; param_1 != 0; param_1 = param_1 + -1) {
        iVar14 = iVar14 + iVar5;
        if (iVar14 < 0) {
          iVar14 = iVar14 + -0x7000;
          iVar11 = iVar11 + 0x700;
        }
      }
      pcVar15 = (char *)(iVar14 + local_2c + local_24);
      local_24 = local_24 * iVar8;
      iVar14 = -1;
LAB_1e71_0133:
      local_6 = 0;
      iVar14 = iVar14 + 1;
      if (iVar14 < iVar7 + iVar13) {
        if (iVar7 <= iVar14) {
          iVar12 = 0;
          pcVar1 = pcVar18;
          pcVar18 = pcVar18 + 1;
          if (*pcVar1 == -1) {
            local_6 = -1;
          }
          else {
            pcVar16 = pcVar15;
            if (*pcVar1 == -3) {
              while (iVar12 < local_20) {
                pcVar17 = pcVar18 + 1;
                cVar6 = *pcVar18;
                if (cVar6 == -1) goto LAB_1e71_016f;
                pcVar18 = pcVar18 + 2;
                cVar3 = *pcVar17;
                do {
                  if ((local_24 <= iVar12) && (iVar12 < local_20)) {
                    if ((cVar3 != -3) && (*pcVar16 == '\0')) {
                      *pcVar16 = cVar3;
                    }
                    pcVar16 = pcVar16 + iVar8;
                  }
                  iVar12 = iVar12 + 1;
                  cVar6 = cVar6 + -1;
                } while (cVar6 != '\0');
              }
            }
            else {
              while (iVar12 < local_20) {
                pcVar17 = pcVar18 + 1;
                cVar6 = *pcVar18;
                if (cVar6 == -1) goto LAB_1e71_016f;
                if (cVar6 == -2) {
                  pcVar2 = pcVar18 + 2;
                  cVar6 = *pcVar17;
                  pcVar18 = pcVar18 + 3;
                  cVar3 = *pcVar2;
                  do {
                    if ((local_24 <= iVar12) && (iVar12 < local_20)) {
                      if ((cVar3 != -3) && (*pcVar16 == '\0')) {
                        *pcVar16 = cVar3;
                      }
                      pcVar16 = pcVar16 + iVar8;
                    }
                    iVar12 = iVar12 + 1;
                    cVar6 = cVar6 + -1;
                  } while (cVar6 != '\0');
                }
                else {
                  if ((local_24 <= iVar12) && (iVar12 < local_20)) {
                    if ((cVar6 != -3) && (*pcVar16 == '\0')) {
                      *pcVar16 = cVar6;
                    }
                    pcVar16 = pcVar16 + iVar8;
                  }
                  iVar12 = iVar12 + 1;
                  pcVar18 = pcVar17;
                }
              }
            }
          }
          goto LAB_1e71_01f2;
        }
        goto LAB_1e71_0204;
      }
    }
  }
  return;
LAB_1e71_016f:
  local_6 = -1;
  pcVar18 = pcVar17;
LAB_1e71_01f2:
  pcVar15 = pcVar15 + iVar5;
  if ((int)pcVar15 < 0) {
    pcVar15 = pcVar15 + -0x7000;
    iVar11 = iVar11 + 0x700;
  }
LAB_1e71_0204:
  if (local_6 != -1) {
    do {
      pcVar1 = pcVar18;
      pcVar18 = pcVar18 + 1;
    } while (*pcVar1 != -1);
  }
  goto LAB_1e71_0133;
}



void __stdcall16far FUN_1e92_000a(int param_1,int param_2,int param_3,undefined2 param_4)

{
  char *pcVar1;
  char *pcVar2;
  char cVar3;
  undefined2 uVar4;
  int iVar5;
  uint in_AX;
  int iVar6;
  char cVar7;
  int iVar8;
  int iVar9;
  int in_DX;
  int iVar10;
  int iVar11;
  int iVar12;
  int *in_BX;
  int iVar13;
  char *pcVar14;
  char *pcVar15;
  char *pcVar16;
  char *pcVar17;
  int iVar18;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int local_172;
  int local_16a;
  int local_162;
  int local_156;
  char local_154 [320];
  uint local_14;
  int local_12;
  undefined2 *local_10;
  undefined2 local_e;
  int local_c;
  uint local_8;
  int local_6;
  int local_4;
  
  iVar8 = in_BX[1];
  iVar12 = *in_BX;
  local_12 = 1;
  if ((int)in_AX < 0) {
    local_12 = -1;
  }
  param_3 = (in_AX & 0x7fff) * 0xc + param_3;
  local_10 = (undefined2 *)(param_3 + 0x36);
  local_e = param_4;
  pcVar17 = (char *)*(undefined2 *)(param_3 + 0x36);
  uVar4 = *(undefined2 *)(param_3 + 0x38);
  iVar13 = in_BX[2];
  iVar11 = in_BX[3];
  local_4 = in_BX[1];
  iVar9 = *(int *)(param_3 + 0x3e);
  iVar5 = *(int *)(param_3 + 0x40);
  iVar10 = iVar9;
  if (iVar9 < iVar5) {
    iVar10 = iVar5;
  }
  iVar18 = 0;
  local_14 = 0;
  local_156 = 0;
  iVar6 = 0x32;
  do {
    iVar6 = iVar6 + param_1;
    if (iVar6 < 100) {
      local_154[iVar18] = '\0';
    }
    else {
      local_154[iVar18] = -1;
      iVar6 = iVar6 + -100;
      if (iVar18 < iVar9) {
        local_14 = local_14 + 1;
      }
      if (iVar18 < iVar5) {
        local_156 = local_156 + 1;
      }
    }
    iVar18 = iVar18 + 1;
  } while (iVar18 < iVar10);
  local_172 = in_DX - (local_14 >> 1);
  iVar9 = (param_2 - local_156) + 1;
  local_16a = 0;
  iVar10 = local_172 + local_14 + -1;
  local_8 = local_14;
  if (local_172 < 0) {
    local_8 = local_14 + local_172;
    local_16a = -local_172;
  }
  iVar6 = iVar10 - (iVar8 + -1);
  if (iVar6 != 0 && iVar8 + -1 <= iVar10) {
    local_8 = local_8 - iVar6;
  }
  local_162 = local_16a + local_8;
  if (0 < (int)local_8) {
    if (local_12 != 1) {
      local_172 = local_172 + local_14 + -1;
      local_16a = -(local_14 - local_162);
      local_162 = (local_14 - local_162) + local_8;
    }
    iVar8 = 0;
    iVar10 = iVar9 + local_156 + -1;
    if (iVar9 < 0) {
      local_156 = local_156 + iVar9;
      iVar8 = -iVar9;
    }
    iVar6 = iVar10 - (iVar12 + -1);
    if (iVar6 != 0 && iVar12 + -1 <= iVar10) {
      local_156 = local_156 - iVar6;
    }
    local_c = local_156;
    if (0 < local_156) {
      for (iVar9 = iVar8 + iVar9; iVar9 != 0; iVar9 = iVar9 + -1) {
        iVar13 = iVar13 + local_4;
        if (iVar13 < 0) {
          iVar13 = iVar13 + -0x7000;
          iVar11 = iVar11 + 0x700;
        }
      }
      pcVar14 = (char *)(iVar13 + local_172 + local_16a);
      local_16a = local_16a * local_12;
      iVar13 = -1;
      iVar12 = -1;
LAB_1e92_01b4:
      local_6 = 0;
      iVar9 = iVar12 + 1;
      if (iVar9 < iVar5) {
        if (local_154[iVar12 + 1] == '\0') goto LAB_1e92_02d3;
        iVar13 = iVar13 + 1;
        if (iVar13 < iVar8 + local_156) {
          if (iVar13 < iVar8) goto LAB_1e92_02d3;
          iVar10 = 0;
          iVar12 = 0;
          pcVar1 = pcVar17;
          pcVar17 = pcVar17 + 1;
          if (*pcVar1 == -1) {
            local_6 = -1;
          }
          else {
            pcVar15 = pcVar14;
            if (*pcVar1 == -3) {
              while (iVar12 < local_162) {
                pcVar16 = pcVar17 + 1;
                cVar7 = *pcVar17;
                if (cVar7 == -1) goto LAB_1e92_020d;
                pcVar17 = pcVar17 + 2;
                cVar3 = *pcVar16;
                do {
                  if (local_154[iVar10] != '\0') {
                    if ((local_16a <= iVar12) && (iVar12 < local_162)) {
                      if ((cVar3 != -3) && (*pcVar15 == '\0')) {
                        *pcVar15 = cVar3;
                      }
                      pcVar15 = pcVar15 + local_12;
                    }
                    iVar12 = iVar12 + 1;
                  }
                  iVar10 = iVar10 + 1;
                  cVar7 = cVar7 + -1;
                } while (cVar7 != '\0');
              }
            }
            else {
              while (iVar12 < local_162) {
                pcVar16 = pcVar17 + 1;
                cVar7 = *pcVar17;
                if (cVar7 == -1) goto LAB_1e92_020d;
                if (cVar7 == -2) {
                  pcVar2 = pcVar17 + 2;
                  cVar7 = *pcVar16;
                  pcVar17 = pcVar17 + 3;
                  cVar3 = *pcVar2;
                  do {
                    if (local_154[iVar10] != '\0') {
                      if ((local_16a <= iVar12) && (iVar12 < local_162)) {
                        if ((cVar3 != -3) && (*pcVar15 == '\0')) {
                          *pcVar15 = cVar3;
                        }
                        pcVar15 = pcVar15 + local_12;
                      }
                      iVar12 = iVar12 + 1;
                    }
                    iVar10 = iVar10 + 1;
                    cVar7 = cVar7 + -1;
                  } while (cVar7 != '\0');
                }
                else {
                  if (local_154[iVar10] != '\0') {
                    if ((local_16a <= iVar12) && (iVar12 < local_162)) {
                      if ((cVar7 != -3) && (*pcVar15 == '\0')) {
                        *pcVar15 = cVar7;
                      }
                      pcVar15 = pcVar15 + local_12;
                    }
                    iVar12 = iVar12 + 1;
                  }
                  iVar10 = iVar10 + 1;
                  pcVar17 = pcVar16;
                }
              }
            }
          }
          goto LAB_1e92_02c0;
        }
      }
    }
  }
  return;
LAB_1e92_020d:
  local_6 = -1;
  pcVar17 = pcVar16;
LAB_1e92_02c0:
  pcVar14 = pcVar14 + local_4;
  if ((int)pcVar14 < 0) {
    pcVar14 = pcVar14 + -0x7000;
    iVar11 = iVar11 + 0x700;
  }
LAB_1e92_02d3:
  iVar12 = iVar9;
  if (local_6 != -1) {
    do {
      pcVar1 = pcVar17;
      pcVar17 = pcVar17 + 1;
    } while (*pcVar1 != -1);
  }
  goto LAB_1e92_01b4;
}



undefined2 __stdcall16far FUN_1ec0_000a(undefined2 param_1,undefined2 param_2)

{
  int iVar1;
  undefined2 in_BX;
  undefined2 unaff_DS;
  long lVar2;
  undefined2 local_4;
  
  local_4 = 1;
  iVar1 = FUN_1297_0104(in_BX,unaff_DS);
  if (iVar1 != 0) {
    lVar2 = FUN_1bca_0000(1,0,param_1,param_2);
    if (lVar2 != 0) {
      local_4 = 0;
    }
  }
  if (iVar1 != 0) {
    FUN_2388_02c2(iVar1);
  }
  return local_4;
}



void __cdecl16far FUN_1ec5_0008(undefined2 param_1,int param_2)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x49c6 = param_1;
  if (param_2 != 0) {
    pcVar1 = (code *)swi(0x10);
    (*pcVar1)();
  }
  return;
}



void __cdecl16far
FUN_1ec5_0024(undefined4 param_1,int param_2,uint param_3,int param_4,int param_5,uint param_6,
             uint param_7)

{
  undefined2 *puVar1;
  undefined2 *puVar2;
  long lVar3;
  uint uVar4;
  int iVar5;
  uint uVar6;
  uint uVar7;
  undefined2 *puVar8;
  int iVar9;
  undefined2 *puVar10;
  undefined2 uVar11;
  
  uVar11 = (undefined2)((ulong)param_1 >> 0x10);
  iVar9 = (int)param_1;
  lVar3 = (ulong)param_3 * (ulong)*(uint *)(iVar9 + 2);
  uVar4 = (uint)lVar3;
  iVar5 = *(int *)(iVar9 + 6) + (int)((ulong)lVar3 >> 0x10) * 0x1000 + (uVar4 >> 4);
  puVar8 = (undefined2 *)(*(int *)(iVar9 + 4) + (uVar4 & 0xf) + param_2);
  puVar10 = (undefined2 *)(param_4 + param_5 * 0x140);
  iVar9 = *(uint *)(iVar9 + 2) - param_6;
  if (param_7 != 0) {
    uVar7 = param_6 >> 1;
    uVar4 = uVar7;
    if ((param_6 & 1) == 0) {
      while (uVar6 = uVar7, uVar4 != 0) {
        for (; uVar6 != 0; uVar6 = uVar6 - 1) {
          puVar2 = puVar10;
          puVar10 = puVar10 + 1;
          puVar1 = puVar8;
          puVar8 = puVar8 + 1;
          *puVar2 = *puVar1;
        }
        puVar8 = (undefined2 *)((int)puVar8 + iVar9);
        if ((int)puVar8 < 0) {
          puVar8 = puVar8 + -0x4000;
          iVar5 = iVar5 + 0x800;
        }
        puVar10 = (undefined2 *)((int)puVar10 + -param_6 + 0x140);
        param_7 = param_7 - 1;
        uVar4 = param_7;
      }
    }
    else {
      do {
        uVar4 = uVar7;
        if (uVar7 != 0) {
          for (; uVar4 != 0; uVar4 = uVar4 - 1) {
            puVar2 = puVar10;
            puVar10 = puVar10 + 1;
            puVar1 = puVar8;
            puVar8 = puVar8 + 1;
            *puVar2 = *puVar1;
          }
        }
        *(undefined1 *)puVar10 = *(undefined1 *)puVar8;
        puVar8 = (undefined2 *)((int)puVar8 + iVar9 + 1);
        puVar10 = (undefined2 *)((int)puVar10 + -param_6 + 0x141);
        param_7 = param_7 - 1;
      } while (param_7 != 0);
    }
  }
  return;
}



undefined2 __cdecl16near FUN_1ed0_0008(void)

{
  undefined2 in_AX;
  int iVar1;
  int iVar2;
  int in_DX;
  undefined2 in_BX;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int local_2e;
  undefined2 local_2c;
  char local_2a [40];
  
  local_2c = 0xffff;
  iVar1 = FUN_1297_0104(in_AX,unaff_DS);
  if (iVar1 != 0) {
    for (local_2e = 1; local_2e <= in_DX; local_2e = local_2e + 1) {
      if (((*(byte *)(iVar1 + 6) & 0x10) != 0) ||
         (iVar2 = FUN_2388_078a(local_2a,0x24,iVar1), iVar2 == 0)) goto LAB_1ed0_0095;
    }
    for (local_2e = 0; iVar2 = FUN_2388_0684(local_2a), local_2e < iVar2; local_2e = local_2e + 1) {
      if (local_2a[local_2e] < ' ') {
        local_2a[local_2e] = '\0';
      }
    }
    FUN_2388_0626(in_BX,local_2a);
    local_2c = 0;
  }
LAB_1ed0_0095:
  if (iVar1 != 0) {
    FUN_2388_02c2(iVar1);
  }
  return local_2c;
}



void __cdecl16far FUN_1ed0_00ac(void)

{
  int iVar1;
  int iVar2;
  undefined2 in_BX;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int local_58;
  int local_56;
  char local_52 [80];
  
  iVar1 = FUN_1297_0104(in_BX,unaff_DS);
  if (iVar1 != 0) {
    local_56 = -1;
    do {
      if ((*(byte *)(iVar1 + 6) & 0x10) != 0) break;
      iVar2 = FUN_2388_078a(local_52,0x4f,iVar1);
      if (iVar2 == 0) break;
      local_58 = 0;
      while( true ) {
        iVar2 = FUN_2388_0684(local_52);
        if (iVar2 <= local_58) break;
        if (local_52[local_58] < ' ') {
          local_52[local_58] = '\0';
        }
        local_58 = local_58 + 1;
      }
      iVar2 = FUN_2388_06fe(local_52,0x3b85,3);
      if (iVar2 == 0) {
        local_56 = 0;
      }
      else {
        FUN_227b_0000(local_52,unaff_SS);
      }
    } while (local_56 != 0);
  }
  if (iVar1 != 0) {
    FUN_2388_02c2(iVar1);
  }
  return;
}



void FUN_1ed0_0156(int param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  code *pcVar1;
  int iVar2;
  undefined2 in_DX;
  undefined2 uVar3;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 *puVar4;
  undefined1 local_52 [80];
  
  iVar2 = FUN_2115_0119();
  if (iVar2 != 0) {
    FUN_2127_0004(0);
    FUN_2127_0059(0x2127);
  }
  FUN_1d1c_01f3();
  FUN_1ffe_01c7();
  FUN_1f3e_000c();
  FUN_1f65_0086(0,3);
  uVar3 = 0x1c3c;
  FUN_1c3c_000a();
  pcVar1 = (code *)swi(0x10);
  (*pcVar1)();
  if (param_1 != -0x14) {
    puVar4 = local_52;
    FUN_2388_0626(puVar4,0x3b89);
    FUN_2388_05e6(local_52,puVar4);
    uVar3 = 0x3b91;
    FUN_2388_05e6(local_52,0x3b91);
    FUN_2388_05e6(local_52,uVar3);
    FUN_2388_05e6(local_52,0x3b9f);
    FUN_2388_05e6(local_52,in_DX);
    FUN_2388_05e6(local_52,0x3ba8);
    FUN_2388_05e6(local_52,param_4);
    FUN_227b_0000(local_52,unaff_SS);
    uVar3 = 0x2388;
    iVar2 = FUN_2388_0684(0x3b32);
    if (iVar2 != 0) {
      uVar3 = 0x227b;
      FUN_227b_0000(0x3b32,unaff_DS);
    }
    if (*(char *)0x74b != '\0') {
      FUN_2388_0758(uVar3,*(undefined2 *)0x74e,*(undefined2 *)0x750,in_DX,10);
      FUN_2388_0758(0x2388,*(undefined2 *)0x752,*(undefined2 *)0x754,param_4,10);
      FUN_2388_0626(local_52,0x3baa);
      FUN_2388_05e6(local_52,in_DX);
      FUN_2388_05e6(local_52,0x3bbd);
      FUN_2388_05e6(local_52,param_4);
      FUN_2388_05e6(local_52,0x3bcf);
      FUN_227b_0000(local_52,unaff_SS);
    }
    FUN_227b_0000(0x3be1,unaff_DS);
    FUN_2388_0626(local_52,0x3be3);
    FUN_2388_0758(0x2388,param_2,param_3,in_DX,10);
    FUN_2388_05e6(local_52,in_DX);
    FUN_227b_0000(local_52,unaff_SS);
    FUN_2388_0626(local_52,0x3bfe);
    FUN_2388_073c(*(undefined2 *)0x4568,in_DX,10);
    FUN_2388_05e6(local_52,in_DX);
    FUN_227b_0000(local_52,unaff_SS);
    FUN_2388_0626(local_52,0x3c19);
    uVar3 = FUN_22aa_001b(in_DX,10);
    FUN_2388_073c(uVar3);
    FUN_2388_05e6(local_52,in_DX);
    FUN_227b_0000(local_52,unaff_SS);
    uVar3 = 0x227b;
    FUN_227b_0000(0x3c34,unaff_DS);
  }
  FUN_1ed0_00ac();
  if (*(int *)0x3b2e != 0 || *(int *)0x3b2c != 0) {
    (*(code *)*(undefined2 *)0x3b2c)(uVar3);
  }
  if (*(int *)0x3b2a != 0 || *(int *)0x3b28 != 0) {
    (*(code *)*(undefined2 *)0x3b28)(uVar3);
  }
  FUN_2388_01db(3);
  return;
}



void __stdcall16far
FUN_1ed0_03d6(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  undefined2 in_AX;
  undefined2 uVar1;
  int in_DX;
  undefined2 extraout_DX;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined1 *puVar3;
  undefined1 local_1c [12];
  undefined1 local_10 [12];
  undefined2 local_4;
  
  local_4 = 0;
  if (*(int *)0x3b30 <= in_DX) {
    FUN_2388_073c();
    FUN_2388_073c();
    FUN_2388_0758(0x2388,param_3,param_4,local_10,10);
    FUN_2388_0758(0x2388,param_1,param_2,local_1c,10);
    FUN_1ed0_0008();
    FUN_1ed0_0008();
    puVar3 = local_1c;
    uVar2 = extraout_DX;
    uVar1 = FUN_1d08_0082(puVar3);
    FUN_1ed0_0156(in_AX,uVar1,uVar2,puVar3);
  }
  return;
}



void __cdecl16far FUN_1f16_000e(void)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x448c = 1;
  *(undefined1 *)0x448b = 0xff;
  *(undefined2 *)0x4496 = 6;
  *(undefined2 *)0x4498 = 0x2168;
  *(undefined2 *)0x449a = 0;
  *(undefined2 *)0x449c = 0x21a7;
  *(undefined2 *)0x449e = 0x24e;
  *(undefined2 *)0x44a0 = 0x21a7;
  *(undefined2 *)0x44a2 = 0x45a;
  *(undefined2 *)0x44a4 = 0x21a7;
  return;
}



void __cdecl16far FUN_1f16_00e6(void)

{
  undefined1 *puVar1;
  undefined1 *puVar2;
  uint uVar3;
  uint uVar4;
  int in_DX;
  int iVar5;
  int iVar6;
  undefined1 *puVar7;
  undefined1 *puVar8;
  undefined1 *puVar9;
  undefined2 unaff_DS;
  uint local_c;
  int local_a;
  int local_4;
  
  if (((*(int *)0x3c64 != 0) &&
      (iVar5 = *(int *)0x4b10, *(int *)0x3c66 = *(int *)0x3c66 + 1, iVar5 <= *(int *)0x3c66)) &&
     (*(int *)0x3ab6 == 0)) {
    uVar3 = FUN_1d18_0006();
    local_4 = 0;
    for (local_a = 0; local_a < *(int *)0x5afe; local_a = local_a + 1) {
      iVar6 = local_a * 4;
      iVar5 = *(int *)(iVar6 + 0x52ea) +
              (uint)CARRY2((uint)*(byte *)(iVar6 + 0x5b03),*(uint *)(iVar6 + 0x52e8));
      if ((iVar5 <= in_DX) &&
         ((iVar5 < in_DX || ((uint)*(byte *)(iVar6 + 0x5b03) + *(uint *)(iVar6 + 0x52e8) <= uVar3)))
         ) {
        *(uint *)(iVar6 + 0x52e8) = uVar3;
        *(int *)(iVar6 + 0x52ea) = in_DX;
        uVar4 = (uint)*(byte *)(iVar6 + 0x5b00);
        local_c = (uint)*(byte *)(iVar6 + 0x5b01);
        local_4 = -1;
        if (1 < *(byte *)(iVar6 + 0x5b00)) {
          puVar7 = (undefined1 *)(uVar4 * 3 + (uint)*(byte *)(iVar6 + 0x5b02) * 3 + 0x6047);
          puVar9 = (undefined1 *)0x49a8;
          puVar8 = puVar7;
          for (iVar5 = 3; iVar5 != 0; iVar5 = iVar5 + -1) {
            puVar2 = puVar9;
            puVar9 = puVar9 + -1;
            puVar1 = puVar8;
            puVar8 = puVar8 + -1;
            *puVar2 = *puVar1;
          }
          for (iVar5 = uVar4 * 3 + -3; iVar5 != 0; iVar5 = iVar5 + -1) {
            puVar2 = puVar7;
            puVar7 = puVar7 + -1;
            puVar1 = puVar8;
            puVar8 = puVar8 + -1;
            *puVar2 = *puVar1;
          }
          puVar8 = (undefined1 *)0x49a8;
          for (iVar5 = 3; iVar5 != 0; iVar5 = iVar5 + -1) {
            puVar2 = puVar7;
            puVar7 = puVar7 + -1;
            puVar1 = puVar8;
            puVar8 = puVar8 + -1;
            *puVar2 = *puVar1;
          }
          local_c = local_c + 1;
          if (uVar4 <= local_c) {
            local_c = 0;
          }
        }
        *(undefined1 *)(local_a * 4 + 0x5b01) = (undefined1)local_c;
      }
    }
    if (local_4 != 0) {
      FUN_2061_001e(0x6048,unaff_DS);
    }
    *(undefined2 *)0x3c66 = 0;
  }
  return;
}



undefined2 __cdecl16far FUN_1f36_000a(void)

{
  code *pcVar1;
  char cVar2;
  int iVar3;
  int in_DX;
  int extraout_DX;
  undefined2 in_BX;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  undefined4 uVar4;
  
  *(undefined2 *)0x3c68 = 0;
  if (*(int *)0x3c6a == 0) {
    pcVar1 = (code *)swi(0x2f);
    cVar2 = (*pcVar1)();
    in_DX = extraout_DX;
    if (cVar2 == -0x80) {
      pcVar1 = (code *)swi(0x2f);
      (*pcVar1)();
      *(undefined2 *)0x3c6c = in_BX;
      *(undefined2 *)0x3c6e = unaff_ES;
      uVar4 = (*(code *)*(undefined2 *)0x3c6c)();
      in_DX = (int)((ulong)uVar4 >> 0x10);
      *(uint *)0x5308 = (uint)uVar4;
      if (0x1ff < (uint)uVar4) {
        *(undefined2 *)0x3c68 = 0xffff;
      }
    }
  }
  *(undefined2 *)0x762 = 0xffff;
  *(undefined2 *)0x764 = 0xffff;
  *(undefined1 *)0x74a = 0;
  if (*(int *)0x3c68 != 0) {
    iVar3 = FUN_2281_0008(1,0);
    if (in_DX != 0 || iVar3 != 0) {
      *(int *)0x3c70 = in_DX + -1;
      FUN_2281_004a(iVar3,in_DX);
    }
  }
  return *(undefined2 *)0x3c68;
}



void __cdecl16far FUN_1f3e_000c(void)

{
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = FUN_2290_0006();
  if (iVar1 == 0) {
    iVar1 = 0;
    do {
      FUN_2292_000a();
      if (*(char *)*(undefined4 *)0x4b56 == '\x04') {
        FUN_228b_0040();
      }
      iVar1 = iVar1 + 1;
    } while (iVar1 < 0x96);
  }
  if (*(char *)0x44fa == '\x04') {
    FUN_228b_0040();
  }
  *(undefined2 *)0x4504 = 0;
  FUN_2289_0000();
  FUN_212d_00ba();
  return;
}



void __cdecl16far FUN_1f3e_005a(void)

{
  FUN_212d_0006();
  FUN_2145_0036();
  FUN_1f36_000a();
  FUN_2297_000e();
  return;
}



bool __stdcall16far
FUN_1f45_0000(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined1 *param_5)

{
  int iVar1;
  undefined2 uVar2;
  bool bVar3;
  undefined1 in_AL;
  int iVar4;
  undefined1 *puVar5;
  undefined2 uVar6;
  undefined4 uVar7;
  
  *param_5 = in_AL;
  uVar7 = FUN_1cc9_0136(param_1,param_2);
  iVar4 = (int)((ulong)uVar7 >> 0x10);
  uVar6 = (undefined2)((ulong)param_5 >> 0x10);
  puVar5 = (undefined1 *)param_5;
  *(int *)(puVar5 + 2) = (int)uVar7;
  *(int *)(puVar5 + 4) = iVar4;
  iVar1 = *(int *)(puVar5 + 2);
  bVar3 = iVar4 != 0;
  if (bVar3 || iVar1 != 0) {
    puVar5[1] = 1;
    uVar2 = *(undefined2 *)(puVar5 + 4);
    *(undefined2 *)(puVar5 + 6) = *(undefined2 *)(puVar5 + 2);
    *(undefined2 *)(puVar5 + 8) = uVar2;
    *(undefined2 *)(puVar5 + 0xe) = param_3;
    *(undefined2 *)(puVar5 + 0x10) = param_4;
    *(undefined2 *)(puVar5 + 10) = param_3;
    *(undefined2 *)(puVar5 + 0xc) = param_4;
  }
  else {
    uVar6 = FUN_1d08_0082(param_3,param_4);
    FUN_1ed0_03d6(uVar6,iVar4,param_3,param_4);
  }
  return !bVar3 && iVar1 == 0;
}



void __stdcall16far
FUN_1f45_0090(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined1 *param_5)

{
  undefined1 in_AL;
  undefined1 *puVar1;
  undefined2 uVar2;
  
  uVar2 = (undefined2)((ulong)param_5 >> 0x10);
  puVar1 = (undefined1 *)param_5;
  puVar1[1] = 0;
  *param_5 = in_AL;
  *(undefined2 *)(puVar1 + 6) = param_3;
  *(undefined2 *)(puVar1 + 8) = param_4;
  *(undefined2 *)(puVar1 + 2) = param_3;
  *(undefined2 *)(puVar1 + 4) = param_4;
  *(undefined2 *)(puVar1 + 0xe) = param_1;
  *(undefined2 *)(puVar1 + 0x10) = param_2;
  *(undefined2 *)(puVar1 + 10) = param_1;
  *(undefined2 *)(puVar1 + 0xc) = param_2;
  return;
}



void __stdcall16far FUN_1f45_00ce(undefined4 param_1)

{
  int iVar1;
  undefined2 uVar2;
  
  uVar2 = (undefined2)((ulong)param_1 >> 0x10);
  iVar1 = (int)param_1;
  if (*(char *)(iVar1 + 1) != '\0') {
    FUN_1cc9_0310(*(undefined2 *)(iVar1 + 2),*(undefined2 *)(iVar1 + 4));
  }
  *(undefined2 *)(iVar1 + 4) = 0;
  *(undefined2 *)(iVar1 + 2) = 0;
  *(undefined2 *)(iVar1 + 0x10) = 0;
  *(undefined2 *)(iVar1 + 0xe) = 0;
  *(undefined2 *)(iVar1 + 0xc) = 0;
  *(undefined2 *)(iVar1 + 10) = 0;
  return;
}



undefined4 __stdcall16far FUN_1f45_010a(undefined4 param_1)

{
  uint *puVar1;
  uint uVar2;
  uint in_AX;
  int in_DX;
  int iVar3;
  undefined2 uVar4;
  undefined2 local_6;
  undefined2 local_4;
  
  local_4 = 0;
  local_6 = 0;
  uVar4 = (undefined2)((ulong)param_1 >> 0x10);
  iVar3 = (int)param_1;
  if ((in_DX < *(int *)(iVar3 + 0x10)) ||
     ((in_DX <= *(int *)(iVar3 + 0x10) && (in_AX <= *(uint *)(iVar3 + 0xe))))) {
    local_6 = *(undefined2 *)(iVar3 + 6);
    local_4 = *(undefined2 *)(iVar3 + 8);
    *(int *)(iVar3 + 6) = *(int *)(iVar3 + 6) + in_AX;
    puVar1 = (uint *)(iVar3 + 0xe);
    uVar2 = *puVar1;
    *puVar1 = *puVar1 - in_AX;
    *(int *)(iVar3 + 0x10) = (*(int *)(iVar3 + 0x10) - in_DX) - (uint)(uVar2 < in_AX);
  }
  else {
    FUN_1ed0_03d6(*(undefined2 *)(iVar3 + 0xe),*(undefined2 *)(iVar3 + 0x10),in_AX,in_DX);
  }
  return CONCAT22(local_4,local_6);
}



undefined4 __stdcall16far FUN_1f45_0174(undefined4 param_1)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  int iVar5;
  code *pcVar6;
  int iVar7;
  undefined2 uVar8;
  uint uVar4;
  
  uVar8 = (undefined2)((ulong)param_1 >> 0x10);
  iVar7 = (int)param_1;
  if ((0 < *(int *)(iVar7 + 0x10)) ||
     ((-1 < *(int *)(iVar7 + 0x10) && (0xf < *(uint *)(iVar7 + 0xe))))) {
    uVar1 = *(uint *)(iVar7 + 10);
    iVar2 = *(int *)(iVar7 + 0xc);
    uVar3 = *(uint *)(iVar7 + 0xe);
    uVar4 = *(uint *)(iVar7 + 0xe);
    iVar5 = *(int *)(iVar7 + 0x10);
    pcVar6 = (code *)swi(0x21);
    (*pcVar6)();
    *(int *)(iVar7 + 10) = uVar1 - uVar4;
    *(int *)(iVar7 + 0xc) = (iVar2 - iVar5) - (uint)(uVar1 < uVar3);
    *(undefined2 *)(iVar7 + 0x10) = 0;
    *(undefined2 *)(iVar7 + 0xe) = 0;
  }
  return CONCAT22(*(undefined2 *)(iVar7 + 0xc),*(undefined2 *)(iVar7 + 10));
}



void __cdecl16far FUN_1f65_0007(void)

{
  char cVar1;
  code *pcVar2;
  char in_AL;
  undefined2 unaff_DI;
  undefined2 unaff_DS;
  char in_stack_00000000;
  
  if (*(int *)0x52de == 0) {
    if (*(int *)0x49c0 != 0) {
      pcVar2 = (code *)swi(0x33);
      (*pcVar2)();
      if (*(char *)0x5ad2 == '\0') {
        return;
      }
      *(char *)0x5ad2 = *(char *)0x5ad2 + '\x01';
      return;
    }
  }
  else {
    cVar1 = *(char *)0x5ad2;
    in_AL = cVar1;
    if ((cVar1 != '\0') && (in_AL = cVar1 + '\x01', (char)(cVar1 + '\x01') == '\0')) {
      FUN_1f65_066f();
      unaff_DS = unaff_DI;
      in_AL = in_stack_00000000;
    }
  }
  *(char *)0x5ad2 = in_AL;
  return;
}



void __cdecl16far FUN_1f65_004e(void)

{
  char *pcVar1;
  char cVar2;
  code *pcVar3;
  undefined2 unaff_DS;
  
  if (*(int *)0x52de == 0) {
    if (*(int *)0x49c0 != 0) {
      pcVar3 = (code *)swi(0x33);
      (*pcVar3)();
      *(char *)0x5ad2 = *(char *)0x5ad2 + -1;
    }
  }
  else {
    pcVar1 = (char *)0x5ad2;
    cVar2 = *pcVar1;
    *pcVar1 = *pcVar1 + -1;
    if (cVar2 == '\0') {
      FUN_1f65_064a();
    }
  }
  return;
}



undefined2 __cdecl16far FUN_1f65_0086(int param_1,undefined2 param_2)

{
  uint uVar1;
  code *pcVar2;
  int iVar3;
  char *in_BX;
  int unaff_ES;
  undefined2 unaff_DS;
  int local_4;
  
  FUN_1ec5_0008(param_2,0);
  *(undefined2 *)0x52e0 = 0x10;
  *(undefined2 *)0x52e2 = 0x10;
  *(undefined2 *)0x52e4 = 0x42d8;
  *(undefined2 *)0x52e6 = 0x25e7;
  *(undefined2 *)0x6046 = param_2;
  *(undefined2 *)0x49c0 = 0;
  *(undefined2 *)0x52de = 0;
  local_4 = 0;
  pcVar2 = (code *)swi(0x21);
  (*pcVar2)();
  if ((unaff_ES != 0 || in_BX != (char *)0x0) && (*in_BX != -0x31)) {
    pcVar2 = (code *)swi(0x33);
    iVar3 = (*pcVar2)();
    if (iVar3 == -1) {
      pcVar2 = (code *)swi(0x33);
      (*pcVar2)();
      iVar3 = -1;
      do {
        pcVar2 = (code *)swi(0x33);
        (*pcVar2)();
        iVar3 = iVar3 + -1;
      } while (iVar3 != 0 && in_BX != (char *)0x0);
      local_4 = -1;
    }
  }
  if (param_1 != 0) {
    *(int *)0x49c0 = local_4;
    *(undefined1 *)0x5ad2 = 0xff;
    *(undefined1 *)0x3c8e = 0;
    *(undefined2 *)0x5e22 = 0xa0;
    *(undefined2 *)0x5e24 = 100;
    *(undefined2 *)0x4c6e = 0;
    *(undefined1 *)0x3caf = 1;
    *(undefined1 *)0x3c89 = 0;
    *(undefined2 *)0x3c87 = *(undefined2 *)0x3c85;
    *(undefined1 *)0x3c9c = 0;
    *(undefined1 *)0x3c9d = 0;
    uVar1 = *(uint *)0x6046;
    *(undefined2 *)0x52de = 0xffff;
    if (uVar1 == 0x13) {
      if (*(int *)0x3c9e == 0) {
        *(undefined1 *)0x3c9c = 1;
      }
      *(undefined2 *)0x43de = 0x657;
      *(undefined2 *)0x43e0 = 0x6e5;
      *(undefined2 *)0x43e2 = 0x891;
      *(undefined2 *)0x43e4 = 0x915;
      *(undefined2 *)0x43e6 = 0x61b;
    }
    else {
      *(undefined2 *)0x52de = 0;
      if (uVar1 == 4) {
        *(undefined1 *)0x3c9c = 1;
        goto LAB_1f65_01eb;
      }
      if ((uVar1 < 4) || (uVar1 == 7)) {
        *(undefined1 *)0x3c9c = 3;
        *(undefined1 *)0x3c9d = 3;
      }
    }
    if (local_4 != 0) {
      if (*(int *)0x3c74 == 0) {
        if (*(int *)0x52de != 0) {
          pcVar2 = (code *)swi(0x33);
          (*pcVar2)();
        }
      }
      else {
        pcVar2 = (code *)swi(0x33);
        (*pcVar2)();
      }
    }
  }
LAB_1f65_01eb:
  return *(undefined2 *)0x49c0;
}



void __cdecl16far FUN_1f65_01f4(uint param_1,uint param_2)

{
  undefined2 unaff_DS;
  
  *(uint *)0x3c94 = param_1 & 0xf;
  *(uint *)0x3c96 = param_2 & 0xf;
  return;
}



void __cdecl16far FUN_1f65_0222(undefined2 param_1)

{
  undefined1 uVar1;
  uint uVar2;
  undefined2 unaff_DS;
  
  if (*(int *)0x49c0 != 0) {
    FUN_1f65_004e();
    DAT_0000_0449 = (char)param_1;
    if (DAT_0000_0449 == '\x03') {
      uVar1 = 0x20;
    }
    else if (DAT_0000_0449 == '\a') {
      uVar1 = 0x30;
    }
    else {
      uVar1 = 0;
    }
    uVar2 = CONCAT11(DAT_0000_0410,uVar1) & 0xcfff;
    DAT_0000_0410 = (byte)(uVar2 >> 8) | (byte)uVar2;
    FUN_1f65_0086(0xffff,param_1);
  }
  return;
}



void __cdecl16far FUN_1f65_02c3(undefined2 param_1)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x3c87 = param_1;
  return;
}



void __cdecl16far FUN_1f65_038a(void)

{
  code *pcVar1;
  undefined2 extraout_DX;
  undefined2 unaff_DS;
  undefined2 in_stack_00000002;
  
  if ((*(int *)0x49c0 != 0) && (*(undefined2 *)0x3c83 = 0xffff, *(char *)0x3c89 != '\0')) {
    FUN_1f65_0222(*(undefined2 *)0x3c7c);
    pcVar1 = (code *)swi(0x33);
    (*pcVar1)(*(undefined2 *)0x3c7f);
    FUN_1f65_055c();
    *(undefined2 *)0x5e22 = in_stack_00000002;
    *(undefined2 *)0x5e24 = extraout_DX;
    if (*(char *)0x3c7e == '\0') {
      FUN_1f65_0007();
    }
    else {
      while (*(char *)0x3c7e != *(char *)0x5ad2) {
        FUN_1f65_004e();
      }
    }
  }
  *(undefined2 *)0x3c87 = *(undefined2 *)0x3c85;
  *(undefined1 *)0x3c89 = 0;
  *(undefined2 *)0x3c83 = 0;
  return;
}



// WARNING: Unable to track spacebase fully for stack
// WARNING: This function may have set the stack pointer

undefined2 __cdecl16far FUN_1f65_03f8(void)

{
  undefined2 uVar1;
  int iVar2;
  undefined2 in_AX;
  uint uVar3;
  uint in_CX;
  undefined2 in_DX;
  undefined2 in_BX;
  undefined2 *puVar4;
  undefined2 *puVar5;
  undefined2 unaff_BP;
  undefined2 unaff_SI;
  undefined2 unaff_DI;
  undefined2 unaff_ES;
  undefined2 unaff_SS;
  undefined2 uVar6;
  byte in_AF;
  byte in_TF;
  byte in_IF;
  byte in_NT;
  
  if (DAT_25e7_42d6 == 0) {
    DAT_25e7_42d6 = 1;
    DAT_25e7_42c0 = 0x25e7;
    DAT_25e7_42be =
         (uint)(in_NT & 1) * 0x4000 | (uint)(in_IF & 1) * 0x200 | (uint)(in_TF & 1) * 0x100 | 0x40 |
         (uint)(in_AF & 1) * 0x10 | 4;
    puVar4 = (undefined2 *)0x42be;
    DAT_25e7_42c2 = unaff_BP;
    DAT_25e7_42c4 = unaff_DI;
    DAT_25e7_42c6 = unaff_SI;
    DAT_25e7_42c8 = unaff_ES;
    DAT_25e7_42ca = in_DX;
    DAT_25e7_42cc = in_CX;
    DAT_25e7_42ce = in_BX;
    DAT_25e7_42d2 = unaff_SS;
    if (DAT_25e7_3c8e == '\0') {
      uVar3 = in_CX >> (DAT_25e7_3c9c & 0x1f);
      if (DAT_25e7_5ad2 == '\0') {
        puVar5 = (undefined2 *)0x42b8;
        DAT_25e7_42b8 = 0xfae8;
        DAT_25e7_42ba = in_DX;
        DAT_25e7_42bc = uVar3;
        DAT_25e7_42d4 = &stack0xfffc;
        FUN_1f65_064a();
        DAT_25e7_5e24 = *puVar5;
        DAT_25e7_5e22 = puVar5[1];
        puVar4 = puVar5 + 1;
        puVar5[1] = 0xfaf5;
        FUN_1f65_066f();
      }
      else {
        DAT_25e7_42d4 = &stack0xfffc;
        if (DAT_25e7_5ad2 != -0x80) {
          DAT_25e7_42d4 = &stack0xfffc;
          DAT_25e7_5e22 = uVar3;
          DAT_25e7_5e24 = in_DX;
        }
      }
    }
    else {
      DAT_25e7_3c8f = 0xff;
      DAT_25e7_3ca0 = in_CX;
      DAT_25e7_3ca2 = in_DX;
      DAT_25e7_42d4 = &stack0xfffc;
    }
    uVar6 = *(undefined2 *)((int)puVar4 + 2);
    uVar1 = *(undefined2 *)0x42d2;
    iVar2 = *(int *)0x42d4;
    *(undefined2 *)0x42d6 = 0;
    return *(undefined2 *)(iVar2 + 2);
  }
  return in_AX;
}



// WARNING: Unable to track spacebase fully for stack
// WARNING: This function may have set the stack pointer

undefined2 __cdecl16far FUN_1f65_045d(void)

{
  undefined2 uVar1;
  int iVar2;
  undefined2 in_AX;
  uint in_CX;
  undefined2 in_DX;
  undefined2 in_BX;
  undefined2 *puVar3;
  undefined2 *puVar4;
  undefined2 unaff_BP;
  undefined2 unaff_SI;
  undefined2 unaff_DI;
  undefined2 unaff_ES;
  undefined2 unaff_SS;
  undefined2 uVar5;
  byte in_AF;
  byte in_TF;
  byte in_IF;
  byte in_NT;
  
  if (DAT_25e7_42d6 != 0) {
    return in_AX;
  }
  DAT_25e7_42d6 = 1;
  DAT_25e7_42c0 = 0x25e7;
  DAT_25e7_42be =
       (uint)(in_NT & 1) * 0x4000 | (uint)(in_IF & 1) * 0x200 | (uint)(in_TF & 1) * 0x100 | 0x40 |
       (uint)(in_AF & 1) * 0x10 | 4;
  DAT_25e7_42bc = in_CX >> (DAT_25e7_3c9c & 0x1f);
  puVar3 = (undefined2 *)0x42b8;
  DAT_25e7_42b8 = 0xfae8;
  DAT_25e7_42ba = in_DX;
  DAT_25e7_42c2 = unaff_BP;
  DAT_25e7_42c4 = unaff_DI;
  DAT_25e7_42c6 = unaff_SI;
  DAT_25e7_42c8 = unaff_ES;
  DAT_25e7_42ca = in_DX;
  DAT_25e7_42cc = in_CX;
  DAT_25e7_42ce = in_BX;
  DAT_25e7_42d2 = unaff_SS;
  DAT_25e7_42d4 = &stack0xfffc;
  FUN_1f65_064a();
  DAT_25e7_5e24 = *puVar3;
  DAT_25e7_5e22 = puVar3[1];
  puVar4 = puVar3 + 1;
  puVar3[1] = 0xfaf5;
  FUN_1f65_066f();
  uVar5 = *(undefined2 *)((int)puVar4 + 2);
  uVar1 = *(undefined2 *)0x42d2;
  iVar2 = *(int *)0x42d4;
  *(undefined2 *)0x42d6 = 0;
  return *(undefined2 *)(iVar2 + 2);
}



void __cdecl16far FUN_1f65_04bf(void)

{
  undefined2 unaff_DS;
  
  if (*(int *)0x52de != 0) {
    *(undefined1 *)0x3c8f = 0;
    *(undefined1 *)0x3c8e = 0xff;
  }
  return;
}



void __cdecl16far FUN_1f65_04d1(void)

{
  char cVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x52de != 0) {
    LOCK();
    cVar1 = *(char *)0x5ad2;
    *(char *)0x5ad2 = -0x80;
    UNLOCK();
    *(undefined1 *)0x3c8e = 0;
    if ((*(char *)0x3c8f != '\0') && (cVar1 == '\0')) {
      FUN_1f65_045d();
    }
    *(char *)0x5ad2 = cVar1;
  }
  return;
}



void __cdecl16far FUN_1f65_0500(int param_1,undefined2 param_2)

{
  code *pcVar1;
  undefined2 in_CX;
  undefined2 unaff_DS;
  
  param_1 = param_1 << 1;
  if (*(int *)0x52de == 0) {
    FUN_1f65_004e();
  }
  else {
    FUN_1f65_04bf(param_2,param_1);
  }
  *(undefined2 *)0x5e22 = in_CX;
  *(undefined2 *)0x5e24 = param_2;
  if (*(int *)0x49c0 != 0) {
    pcVar1 = (code *)swi(0x33);
    (*pcVar1)(param_1);
    if (*(int *)0x52de != 0) {
      FUN_1f65_03f8();
      FUN_1f65_04d1();
      return;
    }
  }
  FUN_1f65_0007();
  return;
}



undefined2 __cdecl16near FUN_1f65_055c(void)

{
  undefined2 uVar1;
  undefined2 in_AX;
  uint in_CX;
  uint in_DX;
  undefined2 unaff_DS;
  
  uVar1 = *(undefined2 *)0x3c9c;
  if (*(int *)0x52de == 0) {
    *(uint *)0x5e22 = in_CX >> ((byte)uVar1 & 0x1f);
    *(uint *)0x5e24 = in_DX >> ((byte)((uint)uVar1 >> 8) & 0x1f);
  }
  return in_AX;
}



uint __cdecl16far FUN_1f65_057c(undefined2 *param_1,undefined2 *param_2)

{
  code *pcVar1;
  undefined2 in_CX;
  undefined2 in_DX;
  undefined2 extraout_DX;
  uint uVar2;
  undefined2 unaff_DS;
  undefined2 uStack_6;
  undefined2 uStack_4;
  
  if (*(int *)0x52de != 0) {
    in_CX = *(undefined2 *)0x5e22;
    in_DX = *(undefined2 *)0x5e24;
    uStack_6 = in_DX;
    uStack_4 = in_CX;
  }
  uVar2 = 0;
  if (*(int *)0x49c0 != 0) {
    pcVar1 = (code *)swi(0x33);
    (*pcVar1)();
    FUN_1f65_055c();
    in_DX = extraout_DX;
  }
  if (*(int *)0x52de != 0) {
    in_CX = uStack_4;
    in_DX = uStack_6;
  }
  *param_1 = in_CX;
  *param_2 = in_DX;
  return uVar2 | *(uint *)0x4c6e;
}



void __cdecl16far FUN_1f65_05bf(void)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x49c0 != 0) {
    pcVar1 = (code *)swi(0x33);
    (*pcVar1)();
  }
  return;
}



void __cdecl16far FUN_1f65_05d7(void)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x49c0 != 0) {
    pcVar1 = (code *)swi(0x33);
    (*pcVar1)();
  }
  return;
}



void FUN_1f65_064a(void)

{
  undefined2 unaff_DS;
  
                    // WARNING: Could not recover jumptable at 0x0001fca3. Too many branches
                    // WARNING: Treating indirect jump as call
  (*(code *)*(undefined2 *)0x43de)();
  return;
}



void FUN_1f65_066f(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 unaff_DI;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x3ca8 = 0;
  *(undefined2 *)0x3caa = 0;
  iVar3 = *(int *)0x5e24;
  iVar2 = *(int *)0x5e22 - *(int *)0x3c94;
  if (iVar2 < 0) {
    *(int *)0x3ca8 = -iVar2;
    iVar2 = 0;
  }
  *(int *)0x3c98 = iVar2;
  iVar3 = iVar3 - *(int *)0x3c96;
  if (iVar3 < 0) {
    *(int *)0x3caa = -iVar3;
    iVar3 = 0;
  }
  *(int *)0x3c9a = iVar3;
  iVar1 = 0x10;
  if (0xb8 < iVar3) {
    iVar1 = 200 - iVar3;
  }
  *(int *)0x3ca4 = iVar1 - *(int *)0x3caa;
  iVar3 = 0x10;
  if (0x130 < iVar2) {
    iVar3 = 0x140 - iVar2;
  }
  *(int *)0x3ca6 = iVar3 - *(int *)0x3ca8;
  FUN_2114_0002();
  *(undefined2 *)0x3c8a = unaff_ES;
  *(undefined2 *)0x3c8c = unaff_DI;
  *(int *)0x3c90 = iVar2;
                    // WARNING: Could not recover jumptable at 0x0001fd31. Too many branches
                    // WARNING: Treating indirect jump as call
  (*(code *)*(undefined2 *)0x43e0)();
  return;
}



void __cdecl16far FUN_1f65_075e(undefined4 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x3cb2 = (int)param_1;
  *(undefined2 *)0x3cb0 = (int)((ulong)param_1 >> 0x10);
  *(undefined2 *)0x3cb4 = param_2;
  return;
}



void __cdecl16far
FUN_1f65_0777(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  undefined2 unaff_DI;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x3cb6 = param_1;
  *(undefined2 *)0x3cba = param_2;
  *(undefined2 *)0x3cb8 = param_3;
  *(undefined2 *)0x3cbc = param_4;
  FUN_2114_0002();
  *(undefined2 *)0x3cc6 = unaff_DI;
  *(undefined2 *)0x3cc8 = param_1;
  return;
}



void __cdecl16far FUN_1f65_07a8(int param_1,int param_2)

{
  undefined2 unaff_DS;
  
  *(int *)0x3cbe = param_1;
  *(int *)0x3cc0 = param_2;
  param_1 = param_2 * *(int *)0x3cb4 + param_1;
  *(int *)0x3cc2 = param_1;
  *(int *)0x3cc4 = param_1 + *(int *)0x3cb2;
  return;
}



undefined2 __cdecl16far FUN_1f65_07cd(void)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  
  if ((*(int *)0x52de != 0) && (*(char *)0x5ad2 == '\0')) {
    if ((*(int *)0x3c98 <= *(int *)0x3cb8) &&
       (((*(int *)0x3cb6 <= (*(int *)0x3c98 - *(int *)0x3ca8) + 0xf &&
         (*(int *)0x3c9a <= *(int *)0x3cbc)) &&
        (*(int *)0x3cba <= (*(int *)0x3c9a - *(int *)0x3caa) + 0xf)))) {
                    // WARNING: Could not recover jumptable at 0x0001fedd. Too many branches
                    // WARNING: Treating indirect jump as call
      uVar1 = (*(code *)*(undefined2 *)0x43e2)();
      return uVar1;
    }
  }
  return 0;
}



void FUN_1f65_0903(void)

{
  undefined2 unaff_DS;
  
                    // WARNING: Could not recover jumptable at 0x0001ff61. Too many branches
                    // WARNING: Treating indirect jump as call
  (*(code *)*(undefined2 *)0x43e4)();
  return;
}



void __cdecl16far FUN_1ffe_01c7(undefined2 param_1,undefined2 param_2)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  if (DAT_1ffe_0034 != '\0') {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    unaff_DS = param_2;
  }
  DAT_1ffe_0034 = 0;
  *(undefined2 *)0x4c6e = 0;
  return;
}



void __stdcall16far FUN_201f_000c(uint param_1,undefined2 param_2)

{
  int iVar1;
  char *pcVar2;
  int iVar3;
  
  FUN_2388_0dd4(param_1,param_2);
  do {
    iVar3 = -1;
    iVar1 = FUN_2388_0dd4(param_1,param_2);
    pcVar2 = (char *)(iVar1 + param_1 + -1);
    if ((*pcVar2 == ' ') || (*pcVar2 == '\t')) {
      *pcVar2 = '\0';
    }
    else {
      iVar3 = 0;
    }
    if ((iVar1 + param_1) - 2 < param_1) {
      iVar3 = 0;
    }
  } while (iVar3 != 0);
  return;
}



uint __stdcall16far FUN_2025_000c(int param_1)

{
  int iVar1;
  undefined2 uVar2;
  int in_DX;
  int iVar3;
  int iVar4;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  long lVar5;
  undefined1 local_2e [40];
  uint local_6;
  int local_4;
  
  local_6 = 0xffff;
  local_4 = 0;
  iVar3 = 0;
  iVar4 = 0;
  do {
    *(undefined1 *)0x43f4 = (char)((*(byte *)0x43f4 + 1) % 10);
    FUN_2388_0626(local_2e,0x43ec);
    FUN_1297_000c(local_2e,unaff_SS);
    if (*(char *)(*(byte *)0x43f4 + 0x43f6) == '\0') {
      iVar3 = -1;
    }
    else {
      iVar4 = iVar4 + 1;
      iVar1 = local_4;
      if (10 < iVar4) goto LAB_2025_00d3;
    }
  } while (iVar3 == 0);
  *(undefined1 *)(*(byte *)0x43f4 + 0x43f6) = 0xff;
  iVar1 = FUN_2388_03a8(local_2e,0x43e8);
  if (iVar1 != 0) {
    iVar4 = 0;
    iVar3 = iVar1;
    if (0 < param_1) {
      do {
        local_4 = iVar3;
        iVar3 = in_DX + iVar4;
        uVar2 = FUN_1c91_0000();
        lVar5 = FUN_1bea_0008(1,0,uVar2,iVar3);
        if (lVar5 == 0) goto LAB_2025_00d3;
        iVar4 = iVar4 + 1;
        iVar3 = local_4;
      } while (iVar4 < param_1);
    }
    local_6 = (uint)*(byte *)0x43f4;
  }
LAB_2025_00d3:
  if (iVar1 != 0) {
    FUN_2388_02c2(iVar1);
  }
  return local_6;
}



void __stdcall16far FUN_2025_00ea(int param_1,undefined2 param_2,int param_3)

{
  int in_AX;
  int iVar1;
  undefined2 uVar2;
  int in_DX;
  int iVar3;
  int iVar4;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  long lVar5;
  undefined1 local_2c [40];
  int local_4;
  
  FUN_2388_0626(local_2c,0x43ec);
  FUN_1297_000c(local_2c,unaff_SS);
  iVar1 = FUN_2388_03a8(local_2c,0x4400);
  if ((iVar1 != 0) && (iVar4 = 0, iVar3 = iVar1, 0 < param_1)) {
    do {
      local_4 = iVar3;
      iVar3 = param_3 + iVar4;
      uVar2 = FUN_1c91_0000();
      lVar5 = FUN_1bca_0000(1,0,uVar2,iVar3);
      if (lVar5 == 0) break;
      iVar4 = iVar4 + 1;
      iVar3 = local_4;
    } while (iVar4 < param_1);
  }
  if ((iVar1 != 0) && (FUN_2388_02c2(iVar1), in_DX == 0)) {
    *(undefined1 *)(in_AX + 0x43f6) = 0;
    FUN_2388_0aa6(0x2388,local_2c);
  }
  return;
}



uint __stdcall16far FUN_203d_000a(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  uint in_AX;
  int in_DX;
  undefined2 *in_BX;
  undefined2 unaff_DS;
  uint local_e;
  uint local_4;
  
  local_4 = 0;
  local_e = in_AX;
  if ((int)in_AX < 0) {
    local_e = FUN_2145_0106();
    local_4 = 0x4000;
  }
  if (-1 < (int)local_e) {
    if (in_DX < 0) {
      FUN_22fb_0006();
      FUN_1c67_0000(param_1,param_2,param_3,200,0x140,*(undefined2 *)0x445c,*(undefined2 *)0x445e,
                    *in_BX,in_BX[1],in_BX[2],in_BX[3]);
    }
    else {
      FUN_2302_000a();
    }
    local_e = local_e | local_4;
    FUN_212d_015e();
  }
  return local_e;
}



uint __stdcall16far FUN_203d_009e(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  uint in_AX;
  int in_DX;
  undefined2 *in_BX;
  undefined2 unaff_DS;
  uint local_e;
  
  local_e = in_AX & 0xbfff;
  if (in_DX < 0) {
    FUN_22fb_0006();
    FUN_1c67_0000(param_1,param_2,param_3,*in_BX,in_BX[1],in_BX[2],in_BX[3],200,0x140,
                  *(undefined2 *)0x445c,*(undefined2 *)0x445e);
  }
  else {
    FUN_2302_000a();
  }
  if ((in_AX >> 8 & 0x40) != 0) {
    FUN_2145_00a0();
  }
  FUN_212d_015e();
  return local_e;
}



ulong __stdcall16far FUN_204f_0002(uint param_1,int param_2)

{
  return CONCAT22(param_2 + (param_1 >> 4),param_1) & 0xffff000f;
}



int __cdecl16far FUN_2050_000a(void)

{
  char cVar1;
  int iVar2;
  char *in_BX;
  int iVar3;
  undefined2 unaff_DS;
  char *local_8;
  
  iVar3 = 0;
  cVar1 = *in_BX;
  do {
    if (cVar1 == '\0') {
      return iVar3;
    }
    iVar2 = (int)*in_BX;
    local_8 = in_BX + 1;
    if ((*(byte *)(iVar2 + 0x45a9) & 2) != 0) {
      iVar2 = iVar2 + -0x20;
    }
    if ((*(byte *)(iVar2 + 0x45a9) & 4) == 0) {
      if ((0x40 < iVar2) && (iVar2 < 0x47)) {
        iVar2 = iVar2 + -0x37;
        goto LAB_2050_0037;
      }
      cVar1 = *local_8;
      while (cVar1 != '\0') {
        local_8 = local_8 + 1;
        cVar1 = *local_8;
      }
    }
    else {
      iVar2 = iVar2 + -0x30;
LAB_2050_0037:
      iVar3 = iVar3 * 0x10 + iVar2;
    }
    cVar1 = *local_8;
    in_BX = local_8;
  } while( true );
}



int __cdecl16far FUN_2056_000e(void)

{
  char cVar1;
  int iVar2;
  char *in_BX;
  int iVar3;
  undefined2 unaff_DS;
  
  iVar3 = 0;
  cVar1 = *in_BX;
  while (cVar1 != '\0') {
    iVar2 = (int)*in_BX;
    in_BX = in_BX + 1;
    if ((*(byte *)(iVar2 + 0x45a9) & 2) != 0) {
      iVar2 = iVar2 + -0x20;
    }
    if ((iVar2 == 0x30) || (iVar2 == 0x31)) {
      iVar3 = iVar3 * 2 + iVar2 + -0x30;
    }
    else {
      cVar1 = *in_BX;
      while (cVar1 != '\0') {
        in_BX = in_BX + 1;
        cVar1 = *in_BX;
      }
    }
    cVar1 = *in_BX;
  }
  return iVar3;
}



undefined2 __cdecl16far FUN_205b_0006(undefined2 param_1)

{
  undefined1 uVar1;
  
  out(0x43,0x36);
  out(0x40,(char)param_1);
  uVar1 = (undefined1)((uint)param_1 >> 8);
  out(0x40,uVar1);
  return CONCAT11(uVar1,uVar1);
}



void __stdcall16far FUN_205c_000a(undefined1 *param_1)

{
  undefined1 *puVar1;
  undefined1 uVar2;
  byte bVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  undefined1 *puVar7;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x3ab6 = 1;
  uVar6 = 0x300;
  puVar7 = (undefined1 *)param_1;
  out(0x3c7,0);
  do {
    do {
      bVar3 = in(0x3da);
    } while ((bVar3 & 8) != 0);
    do {
      bVar3 = in(0x3da);
    } while ((bVar3 & 8) == 0);
    uVar4 = uVar6;
    uVar5 = uVar6;
    if (0x40 < uVar6) {
      uVar4 = 0x40;
      uVar5 = uVar4;
    }
    do {
      puVar1 = puVar7;
      puVar7 = puVar7 + 1;
      uVar2 = in(0x3c9);
      *puVar1 = uVar2;
      uVar4 = uVar4 - 1;
    } while (uVar4 != 0);
    uVar6 = uVar6 - uVar5;
  } while (uVar6 != 0);
  *(undefined2 *)0x3ab6 = 0;
  return;
}



void __cdecl16far FUN_2061_000a(void)

{
  byte bVar1;
  
  do {
    bVar1 = in(0x3da);
  } while ((bVar1 & 8) != 0);
  do {
    bVar1 = in(0x3da);
  } while ((bVar1 & 8) == 0);
  return;
}



void __stdcall16far FUN_2061_001e(undefined4 param_1)

{
  undefined1 *puVar1;
  uint uVar2;
  byte bVar3;
  int in_AX;
  uint uVar4;
  uint uVar5;
  int in_DX;
  undefined1 *puVar6;
  uint uVar7;
  undefined2 unaff_DS;
  
  uVar7 = in_DX * 3;
  *(undefined2 *)0x3ab6 = 0xffff;
  uVar2 = *(uint *)0x3ab4;
  puVar6 = (undefined1 *)((int)param_1 + in_AX * 3);
  out(0x3c8,(char)in_AX);
  do {
    do {
      bVar3 = in(0x3da);
    } while ((bVar3 & 8) != 0);
    do {
      bVar3 = in(0x3da);
    } while ((bVar3 & 8) == 0);
    uVar5 = uVar7;
    uVar4 = uVar7;
    if (uVar2 < uVar7) {
      uVar5 = uVar2;
      uVar4 = uVar2;
    }
    do {
      puVar1 = puVar6;
      puVar6 = puVar6 + 1;
      out(*puVar1,0x3c9);
      uVar5 = uVar5 - 1;
    } while (uVar5 != 0);
    uVar7 = uVar7 - uVar4;
  } while (uVar7 != 0);
  *(undefined2 *)0x3ab6 = 0;
  return;
}



void __stdcall16far FUN_2069_0006(int param_1,undefined2 param_2,undefined4 param_3)

{
  byte *pbVar1;
  byte bVar2;
  int iVar3;
  byte *pbVar4;
  byte *pbVar5;
  undefined2 uVar6;
  int local_c;
  
  local_c = 0;
  while( true ) {
    uVar6 = (undefined2)((ulong)param_3 >> 0x10);
    iVar3 = (int)param_3;
    if (*(int *)(iVar3 + 4) <= local_c) {
      return;
    }
    pbVar5 = (byte *)*(undefined2 *)(iVar3 + local_c * 0xc + 0x42);
    iVar3 = *(int *)(iVar3 + local_c * 0xc + 0x44);
    if (iVar3 == 0 && pbVar5 == (byte *)0x0) break;
    while( true ) {
      pbVar1 = pbVar5;
      pbVar5 = pbVar5 + 1;
      bVar2 = *pbVar1;
      if (bVar2 == 0xfc) break;
      if (bVar2 != 0xff) {
        pbVar4 = pbVar5;
        if (bVar2 == 0xfd) {
          for (; pbVar5 = pbVar4 + 1, *pbVar4 != 0xff; pbVar4 = pbVar4 + 2) {
            if (*pbVar5 != 0xfd) {
              *pbVar5 = *(byte *)((uint)*pbVar5 * 6 + param_1 + 5);
            }
          }
        }
        else {
          for (; bVar2 = *pbVar5, bVar2 != 0xff; pbVar5 = pbVar5 + 1) {
            if (bVar2 == 0xfe) {
              pbVar5 = pbVar5 + 2;
              bVar2 = *pbVar5;
            }
            if (bVar2 != 0xfd) {
              *pbVar5 = *(byte *)((uint)bVar2 * 6 + param_1 + 5);
            }
          }
          pbVar5 = pbVar5 + 1;
        }
      }
    }
    local_c = local_c + 1;
  }
  return;
}



void __cdecl16near FUN_2074_000e(code *param_1,int param_2,undefined2 param_3)

{
  undefined2 unaff_DS;
  
  if (param_1 != (code *)0x0 || param_2 != 0) {
    *(undefined2 *)0x4410 = param_3;
    (*param_1)(0x2074);
    *(undefined2 *)0x4410 = 0;
  }
  return;
}



void __cdecl16far FUN_2074_002c(void)

{
  undefined2 *puVar1;
  undefined2 *puVar2;
  int in_AX;
  int iVar3;
  int in_DX;
  undefined2 *puVar4;
  undefined2 *puVar5;
  undefined2 unaff_DS;
  
  *(undefined2 *)0x4a88 = 0;
  *(undefined2 *)0x49cc = 0x100;
  puVar4 = (undefined2 *)0x6572;
  for (iVar3 = 0x200; iVar3 != 0; iVar3 = iVar3 + -1) {
    puVar1 = puVar4;
    puVar4 = puVar4 + 1;
    *puVar1 = 0;
  }
  if (*(int *)0x440a == 0) {
    FUN_22b0_0000();
    *(undefined2 *)0x440a = 0xffff;
  }
  if (0 < in_AX) {
    *(undefined2 *)0x6572 = 1;
    *(undefined2 *)0x6574 = 0;
    puVar5 = (undefined2 *)0x6576;
    puVar4 = (undefined2 *)0x6572;
    for (iVar3 = (in_AX + -1) * 2; iVar3 != 0; iVar3 = iVar3 + -1) {
      puVar2 = puVar5;
      puVar5 = puVar5 + 1;
      puVar1 = puVar4;
      puVar4 = puVar4 + 1;
      *puVar2 = *puVar1;
    }
  }
  if (0 < in_DX) {
    *(undefined2 *)0x696e = 1;
    *(undefined2 *)0x6970 = 0;
    puVar5 = (undefined2 *)0x696c;
    puVar4 = (undefined2 *)0x6970;
    for (iVar3 = (in_DX + -1) * 2; iVar3 != 0; iVar3 = iVar3 + -1) {
      puVar2 = puVar5;
      puVar5 = puVar5 + -1;
      puVar1 = puVar4;
      puVar4 = puVar4 + -1;
      *puVar2 = *puVar1;
    }
  }
  puVar4 = (undefined2 *)0x5294;
  for (iVar3 = 0x20; iVar3 != 0; iVar3 = iVar3 + -1) {
    puVar1 = puVar4;
    puVar4 = puVar4 + 1;
    *puVar1 = 0;
  }
  *(undefined2 *)0x5294 = 0xffff;
  *(undefined2 *)0x5296 = 0xffff;
  *(int *)0x5e20 = in_AX;
  *(int *)0x52d6 = in_DX;
  *(undefined2 *)0x4408 = 0;
  *(undefined2 *)0x4412 = 0;
  FUN_2074_000e(*(undefined2 *)0x440c,*(undefined2 *)0x440e,1);
  return;
}



void __cdecl16far FUN_2074_00ec(void)

{
  int *piVar1;
  undefined2 unaff_DS;
  
  if ((*(int *)0x52d2 != 0) && (*(int *)0x4408 == 0)) {
    FUN_1ed0_03d6(1,0,0x20,0);
  }
  *(undefined2 *)0x4408 = 0xffff;
  *(undefined2 *)0x52d2 = 0xffff;
  piVar1 = (int *)0x6572;
  do {
    if (piVar1[1] != 0 || *piVar1 != 0) {
      *(byte *)((int)piVar1 + 3) = *(byte *)((int)piVar1 + 3) | 0x80;
    }
    piVar1 = piVar1 + 2;
  } while (piVar1 < (int *)0x6972);
  return;
}



int __stdcall16far FUN_2074_0288(char *param_1,char *param_2)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  int iVar4;
  char *pcVar5;
  char *pcVar6;
  
  pcVar6 = (char *)param_2;
  pcVar5 = (char *)param_1;
  iVar3 = 3;
  iVar4 = 0;
  do {
    pcVar2 = pcVar5;
    pcVar5 = pcVar5 + 1;
    pcVar1 = pcVar6;
    pcVar6 = pcVar6 + 1;
    iVar4 = iVar4 + (int)(char)(*pcVar2 - *pcVar1) * (int)(char)(*pcVar2 - *pcVar1);
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  return iVar4;
}



void __stdcall16far FUN_2074_02ae(int param_1,undefined2 param_2,int *param_3)

{
  int iVar1;
  int *piVar2;
  int *piVar3;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  int local_18 [6];
  int *local_c;
  undefined2 local_a;
  int local_8;
  
  local_8 = 0;
  if (0 < *param_3) {
    local_c = (int *)param_3 + 1;
    local_a = param_3._2_2_;
    piVar2 = local_c;
    piVar3 = local_18;
    do {
      iVar1 = FUN_233e_000a(*piVar2 * 6 + param_1 + 2,param_2);
      *piVar3 = iVar1;
      piVar3[1] = iVar1 >> 0xf;
      local_8 = local_8 + 1;
      piVar2 = piVar2 + 1;
      piVar3 = piVar3 + 2;
    } while (local_8 < *param_3);
  }
  FUN_22c7_000e(local_18,unaff_SS,(int *)param_3 + 1,param_3._2_2_);
  return;
}



void __stdcall16far FUN_2074_0332(int *param_1,int *param_2,undefined2 param_3)

{
  int iVar1;
  undefined4 local_a;
  
  iVar1 = 0;
  *param_2 = 0;
  if (0 < *param_1) {
    local_a = (byte *)CONCAT22(param_1._2_2_,(byte *)((int)(int *)param_1 + 7));
    do {
      if (((*local_a & 0x10) != 0) && (*param_2 < 3)) {
        param_2[*param_2 + 1] = iVar1;
        *param_2 = *param_2 + 1;
      }
      local_a = (byte *)CONCAT22(local_a._2_2_,(byte *)local_a + 6);
      iVar1 = iVar1 + 1;
    } while (iVar1 < *param_1);
  }
  return;
}



void __stdcall16far FUN_2074_0392(undefined2 param_1,undefined2 param_2)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x4404 = param_1;
  *(undefined2 *)0x4406 = param_2;
  return;
}



int __cdecl16near FUN_2074_03a6(void)

{
  int iVar1;
  int *piVar2;
  undefined2 unaff_DS;
  
  iVar1 = 0;
  piVar2 = (int *)0x5294;
  do {
    if (*piVar2 == 0) {
      return iVar1;
    }
    iVar1 = iVar1 + 1;
    piVar2 = piVar2 + 1;
  } while (piVar2 < (int *)0x52d4);
  FUN_1ed0_03d6(1,0,0x20,0);
  return -10;
}



int __cdecl16near FUN_2074_03e0(int *param_1)

{
  int iVar1;
  int *piVar2;
  undefined2 unaff_DS;
  int local_4;
  
  *param_1 = -1;
  iVar1 = 0;
  local_4 = 0;
  piVar2 = (int *)0x6572;
  do {
    if ((piVar2[1] == 0 && *piVar2 == 0) && (local_4 = local_4 + 1, *param_1 < 0)) {
      *param_1 = iVar1;
    }
    iVar1 = iVar1 + 1;
    piVar2 = piVar2 + 2;
  } while (piVar2 < (int *)0x6972);
  return local_4;
}



int __stdcall16far FUN_2074_0416(int *param_1,int *param_2)

{
  uint *puVar1;
  int *piVar2;
  uint in_AX;
  int iVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  char cVar7;
  uint uVar8;
  int iVar9;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar10;
  byte local_24f;
  uint local_24e;
  int local_244;
  undefined1 local_23a [4];
  undefined2 local_236;
  undefined2 local_234;
  uint local_232;
  uint local_230;
  int local_22e;
  int local_22c;
  int local_22a;
  int local_228;
  uint local_226 [3];
  byte local_220 [256];
  uint local_120;
  undefined2 local_11e;
  int local_11c;
  undefined1 *local_11a;
  int local_118;
  int local_116;
  int local_114;
  int local_112;
  int local_110;
  byte local_10e [256];
  int local_e;
  int local_c;
  int local_a;
  int local_8;
  int local_6;
  int local_4;
  
  local_11a = local_23a;
  uVar5 = -(uint)((in_AX & 0x800) == 0) & 0xfffc;
  iVar3 = CONCAT11((char)(uVar5 >> 8) + '\x01',(char)uVar5);
  if ((in_AX & 0x4000) == 0) {
    local_8 = *(int *)0x5e20;
    iVar9 = 0x100 - *(int *)0x52d6;
    if (iVar3 < 0x100 - *(int *)0x52d6) {
      iVar9 = iVar3;
    }
  }
  else {
    local_8 = 0;
    iVar9 = iVar3;
  }
  iVar3 = *(int *)0x4a88;
  if (*(int *)0x4a88 < local_8) {
    iVar3 = local_8;
  }
  iVar4 = *(int *)0x49cc;
  if (iVar9 < *(int *)0x49cc) {
    iVar4 = iVar9;
  }
  local_8 = iVar3;
  local_22e = FUN_2074_03a6();
  if (-1 < local_22e) {
    uVar5 = 1;
    uVar8 = 0;
    for (cVar7 = (char)local_22e; cVar7 != '\0'; cVar7 = cVar7 + -1) {
      bVar10 = (int)uVar5 < 0;
      uVar5 = uVar5 << 1;
      uVar8 = uVar8 << 1 | (uint)bVar10;
    }
    local_24f = (byte)(in_AX >> 8);
    local_e = (local_24f & 0x80) * 0x100;
    local_c = (local_24f & 0x84) << 8;
    bVar10 = param_1._2_2_ != 0 || (int *)param_1 != (int *)0x0;
    local_114 = 0;
    if (bVar10) {
      if (((in_AX & 0x8000) != 0) || (*param_1 == 0)) {
        bVar10 = false;
      }
      if (((in_AX & 0x8000) != 0) && (*param_1 != 0)) {
        local_114 = -1;
      }
    }
    if (bVar10) {
      FUN_2074_0332((int *)param_2,param_2._2_2_,&local_228,unaff_SS);
      FUN_2074_02ae((int *)param_2,param_2._2_2_,&local_228,unaff_SS);
    }
    local_a = FUN_2074_03e0(&local_22a);
    iVar3 = local_8;
    if (local_8 < local_22a) {
      iVar3 = local_22a;
    }
    for (local_22c = 0; local_22c < *param_2; local_22c = local_22c + 1) {
      local_220[local_22c] = (byte)local_22c;
      local_10e[local_22c] = 0;
      if ((*(byte *)((int)(int *)param_2 + local_22c * 6 + 7) & 0x80) == 0) {
        local_10e[local_22c] = 0x40;
      }
      if ((*(byte *)((int)(int *)param_2 + local_22c * 6 + 7) & 0x60) != 0) {
        local_10e[local_22c] = local_10e[local_22c] | 0x20;
      }
    }
    local_22a = iVar3;
    FUN_22e0_0008(local_10e,unaff_SS,local_220,unaff_SS);
    if ((in_AX & 0x4000) == 0) {
      local_236 = 0xfffe;
    }
    else {
      local_236 = 0xffff;
    }
    local_234 = 0xffff;
    for (local_116 = 0; local_116 < *param_2; local_116 = local_116 + 1) {
      local_244 = 0;
      local_6 = -1;
      uVar6 = (uint)local_220[local_116];
      if ((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 8) != 0) {
        local_244 = -1;
        local_6 = 0xfd;
      }
      if ((bVar10) && ((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 0x10) != 0)) {
        for (local_11c = 0; (local_244 == 0 && (local_11c < local_228)); local_11c = local_11c + 1)
        {
          if (local_226[local_11c] == uVar6) {
            local_244 = -1;
            iVar3 = *param_1 + -1;
            if (local_11c < *param_1 + -1) {
              iVar3 = local_11c;
            }
            local_6 = ((int *)param_1)[iVar3 + 1];
          }
        }
      }
      if ((local_114 != 0) && ((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 0x10) != 0)) {
        local_11c = 0;
        while (local_244 == 0) {
          piVar2 = (int *)*(undefined4 *)0x4404;
          if (*piVar2 <= local_11c) break;
          if (((int *)piVar2)[local_11c + 1] == uVar6) {
            local_244 = -1;
            local_6 = local_11c + 0xf0;
            FUN_2388_0c4a(local_6 * 3 + 0x5b20,unaff_DS,(int *)param_2 + uVar6 * 3 + 1,param_2._2_2_
                          ,3);
          }
          local_11c = local_11c + 1;
        }
      }
      if ((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 0x80) == 0) {
        local_232 = 2;
      }
      else {
        local_232 = 0;
      }
      local_230 = 0;
      if ((local_244 == 0) && (local_e == 0)) {
        local_118 = 1;
      }
      else {
        local_118 = 0;
      }
      if (local_118 != 0) {
        iVar3 = local_8;
        if (((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 0x20) == 0) &&
           ((((in_AX & 0x2000) == 0 &&
             ((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 0x40) == 0)) ||
            (((in_AX & 0x1000) == 0 && (local_a != 0)))))) {
          local_4 = 1;
        }
        else {
          local_4 = 0x7fff;
        }
        for (; iVar3 < iVar4; iVar3 = iVar3 + 1) {
          iVar9 = iVar3 * 4;
          if ((*(int *)(iVar9 + 0x6574) != 0 || *(int *)(iVar9 + 0x6572) != 0) &&
             ((((*(byte *)(iVar9 + 0x6572) & 1) == 0 || ((in_AX & 0x4000) != 0)) &&
              ((*(uint *)(iVar3 * 4 + 0x6574) & local_230) == 0 &&
               (*(uint *)(iVar3 * 4 + 0x6572) & local_232) == 0)))) {
            if (local_4 < 2) {
              if ((((int *)param_2)[uVar6 * 3 + 1] == *(int *)(iVar3 * 3 + 0x5b20)) &&
                 ((char)((int *)param_2)[uVar6 * 3 + 2] == *(char *)(iVar3 * 3 + 0x5b22))) {
                local_110 = 0;
              }
              else {
                local_110 = 1;
              }
            }
            else {
              local_110 = FUN_2074_0288(iVar3 * 3 + 0x5b20,unaff_DS,(int *)param_2 + uVar6 * 3 + 1,
                                        param_2._2_2_);
            }
            if (local_110 < local_4) {
              local_244 = -1;
              local_4 = local_110;
              local_6 = iVar3;
            }
          }
        }
      }
      if ((local_244 == 0) &&
         (((in_AX & 0x1000) == 0 ||
          (((*(byte *)((int)(int *)param_2 + uVar6 * 6 + 7) & 0x60) == 0 && ((in_AX & 0x2000) == 0))
          )))) {
        local_112 = 1;
      }
      else {
        local_112 = 0;
      }
      iVar3 = local_22a;
      if (local_112 != 0) {
        for (; (local_244 == 0 && (iVar3 < iVar4)); iVar3 = iVar3 + 1) {
          if (*(int *)(iVar3 * 4 + 0x6574) == 0 && *(int *)(iVar3 * 4 + 0x6572) == 0) {
            local_a = local_a + -1;
            local_22a = local_22a + 1;
            local_244 = -1;
            *(int *)(iVar3 * 3 + 0x5b20) = ((int *)param_2)[uVar6 * 3 + 1];
            *(undefined1 *)(iVar3 * 3 + 0x5b22) = (char)((int *)param_2)[uVar6 * 3 + 2];
            local_6 = iVar3;
          }
        }
      }
      if (local_244 == 0) {
        *(int *)0x4412 = *param_2;
        FUN_2074_000e(*(undefined2 *)0x440c,*(undefined2 *)0x440e,3);
        FUN_1ed0_03d6(local_116,local_116 >> 0xf,*param_2,*param_2 >> 0xf);
        return -0xb;
      }
      if ((local_e == 0) || ((char)((int *)param_2)[uVar6 * 3 + 3] == '\0')) {
        local_24e = 0;
      }
      else {
        local_24e = 2;
      }
      local_120 = local_24e;
      local_11e = 0;
      puVar1 = (uint *)(local_6 * 4 + 0x6572);
      *puVar1 = *puVar1 | local_24e | uVar5;
      puVar1 = (uint *)(local_6 * 4 + 0x6574);
      *puVar1 = *puVar1 | uVar8;
      *(undefined1 *)((int)(int *)param_2 + uVar6 * 6 + 5) = (undefined1)local_6;
    }
    *(undefined2 *)(local_22e * 2 + 0x5294) = 0xffff;
    *(int *)0x4412 = *param_2;
    FUN_2074_000e(*(undefined2 *)0x440c,*(undefined2 *)0x440e,2);
  }
  return local_22e;
}



void __cdecl16far FUN_2114_0002(void)

{
  return;
}



void __cdecl16far FUN_2115_00b2(int param_1)

{
  undefined2 uVar1;
  undefined2 *puVar2;
  undefined2 *puVar3;
  int iVar4;
  undefined2 *puVar5;
  undefined2 *puVar6;
  undefined2 unaff_DS;
  
  *(undefined1 *)0x4414 = 0xff;
  if (param_1 != 0) {
    puVar5 = (undefined2 *)0x32;
    puVar6 = (undefined2 *)0x602a;
    iVar4 = 5;
    uVar1 = *(undefined2 *)0x28;
    do {
      puVar3 = puVar6 + 1;
      puVar2 = puVar5;
      puVar5 = puVar5 + 1;
      *puVar6 = *puVar2;
      puVar6 = puVar6 + 2;
      *puVar3 = uVar1;
      iVar4 = iVar4 + -1;
    } while (iVar4 != 0);
  }
  return;
}



void __cdecl16far FUN_2115_00e3(int param_1)

{
  code *pcVar1;
  
  if (param_1 != 0) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    FUN_2115_0100();
  }
  return;
}



void __cdecl16far FUN_2115_0100(void)

{
  int iVar1;
  undefined2 *puVar2;
  undefined2 unaff_DS;
  
  iVar1 = 5;
  puVar2 = (undefined2 *)0x602a;
  do {
    *puVar2 = 0xfd;
    puVar2[1] = 0x2115;
    puVar2 = puVar2 + 2;
    iVar1 = iVar1 + -1;
  } while (iVar1 != 0);
  return;
}



undefined1 __cdecl16far FUN_2115_0119(void)

{
  undefined2 unaff_DS;
  
  return *(undefined1 *)0x4414;
}



void __cdecl16far FUN_2127_0004(undefined2 param_1)

{
  undefined2 unaff_DS;
  
  if (*(char *)0x4437 == '\0') {
                    // WARNING: Could not recover jumptable at 0x0002127b. Too many branches
                    // WARNING: Treating indirect jump as call
    (*(code *)(ulong)*(uint *)0x602e)();
    return;
  }
  if (*(byte *)0x4436 < 8) {
    *(undefined2 *)((char)*(byte *)0x4436 * 2 + 0x4426) = param_1;
    *(char *)0x4436 = *(char *)0x4436 + '\x01';
  }
  return;
}



void FUN_2127_0059(void)

{
  undefined2 unaff_DS;
  
                    // WARNING: Could not recover jumptable at 0x000212c9. Too many branches
                    // WARNING: Treating indirect jump as call
  (*(code *)(ulong)*(uint *)0x6032)();
  return;
}



int __cdecl16far FUN_212d_0006(void)

{
  char *pcVar1;
  char *pcVar2;
  code *pcVar3;
  byte bVar4;
  char extraout_AH;
  uint uVar5;
  char extraout_AH_00;
  char extraout_AH_01;
  int iVar6;
  int iVar7;
  undefined2 extraout_DX;
  int iVar8;
  char *pcVar9;
  char *pcVar10;
  int *piVar11;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  bool bVar12;
  
  *(undefined2 *)0x4438 = 0;
  pcVar3 = (code *)swi(0x21);
  (*pcVar3)();
  pcVar10 = (char *)0xa;
  pcVar9 = (char *)0x446c;
  iVar8 = 0;
  bVar12 = true;
  iVar6 = 8;
  do {
    if (iVar6 == 0) break;
    iVar6 = iVar6 + -1;
    pcVar2 = pcVar10;
    pcVar10 = pcVar10 + 1;
    pcVar1 = pcVar9;
    pcVar9 = pcVar9 + 1;
    bVar12 = *pcVar1 == *pcVar2;
  } while (bVar12);
  if ((bVar12) && (*(int *)0x4446 == 0)) {
    pcVar3 = (code *)swi(0x67);
    (*pcVar3)();
    if (extraout_AH == '\0') {
      pcVar3 = (code *)swi(0x67);
      uVar5 = (*pcVar3)();
      if ((char)(uVar5 >> 8) == '\0') {
        bVar4 = (byte)uVar5 >> 4;
        if (2 < bVar4) {
          *(uint *)0x4442 = (uint)bVar4;
          *(uint *)0x4444 = uVar5 & 0xf;
          pcVar3 = (code *)swi(0x67);
          (*pcVar3)();
          *(int *)0x443c = iVar8;
          iVar7 = 0;
          piVar11 = (int *)0x445c;
          iVar6 = 4;
          do {
            *piVar11 = iVar7;
            piVar11[1] = iVar8;
            iVar7 = iVar7 + 0x4000;
            piVar11 = piVar11 + 2;
            iVar6 = iVar6 + -1;
          } while (iVar6 != 0);
          iVar6 = 0;
          if (extraout_AH_00 == '\0') {
            *(undefined2 *)0x4438 = 0xffff;
            pcVar3 = (code *)swi(0x67);
            (*pcVar3)();
            *(int *)0x4440 = iVar6;
            bVar12 = iVar6 != 0;
            iVar6 = 0;
            if (bVar12) {
              pcVar3 = (code *)swi(0x67);
              (*pcVar3)();
              if (extraout_AH_01 != '\0') {
                return 0;
              }
              *(undefined2 *)0x443e = extraout_DX;
              iVar6 = -1;
              *(undefined2 *)0x443a = 0xffff;
            }
          }
          return iVar6;
        }
      }
    }
  }
  return iVar8;
}



void __cdecl16far FUN_212d_00ba(void)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x443a != 0) {
    pcVar1 = (code *)swi(0x67);
    (*pcVar1)();
  }
  *(undefined2 *)0x443a = 0;
  return;
}



uint __cdecl16far FUN_212d_00d4(int param_1,int param_2)

{
  code *pcVar1;
  uint uVar2;
  undefined2 unaff_DS;
  
  if (param_1 == 1) {
    *(undefined2 *)0x4448 = 0;
  }
  if (*(int *)(param_1 * 2 + 0x444c) != param_2) {
    *(int *)(param_1 * 2 + 0x444c) = param_2;
    *(undefined2 *)0x444a = 0xffff;
    pcVar1 = (code *)swi(0x67);
    uVar2 = (*pcVar1)();
    return uVar2 >> 8;
  }
  return 0;
}



void __cdecl16far FUN_212d_0114(void)

{
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  do {
    *(undefined2 *)(local_4 * 2 + 0x4454) = *(undefined2 *)(local_4 * 2 + 0x444c);
    local_4 = local_4 + 1;
  } while (local_4 < 4);
  return;
}



void __cdecl16far FUN_212d_0136(void)

{
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  do {
    FUN_212d_00d4(local_4,*(undefined2 *)(local_4 * 2 + 0x4454));
    local_4 = local_4 + 1;
  } while (local_4 < 4);
  return;
}



void __cdecl16far FUN_212d_015e(void)

{
  undefined2 unaff_DS;
  undefined2 local_4;
  
  if ((*(int *)0x443a != 0) && (3 < *(uint *)0x4442)) {
    local_4 = 0;
    do {
      FUN_212d_00d4(local_4,0xffff);
      local_4 = local_4 + 1;
    } while (local_4 < 4);
  }
  return;
}



bool __cdecl16far FUN_2145_000c(void)

{
  int iVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x4448 == 0) {
    iVar1 = FUN_212d_00d4(1,*(int *)0x4440 + -1);
    *(uint *)0x4448 = (uint)(iVar1 == 0);
  }
  return *(int *)0x4448 == 0;
}



undefined2 __cdecl16far FUN_2145_0036(void)

{
  undefined1 *puVar1;
  undefined2 uVar2;
  undefined1 *puVar3;
  uint uVar4;
  int iVar5;
  undefined1 *puVar6;
  undefined2 unaff_DS;
  
  if ((*(int *)0x443a != 0) && (1 < *(uint *)0x4440)) {
    uVar4 = *(uint *)0x4440 - 0x800 & -(uint)(*(uint *)0x4440 < 0x800);
    *(undefined2 *)0x4440 = CONCAT11((char)(uVar4 >> 8) + '\b',(char)uVar4);
    uVar2 = *(undefined2 *)0x4462;
    *(undefined2 *)0x447c = *(undefined2 *)0x4460;
    *(undefined2 *)0x447e = uVar2;
    iVar5 = FUN_2145_000c();
    if (iVar5 == 0) {
      puVar3 = (undefined1 *)*(undefined4 *)0x447c;
      puVar6 = (undefined1 *)puVar3;
      for (iVar5 = *(int *)0x4440; iVar5 != 0; iVar5 = iVar5 + -1) {
        puVar1 = puVar6;
        puVar6 = puVar6 + 1;
        *puVar1 = 0;
      }
      iVar5 = *(int *)0x4440;
      *(undefined1 *)((int)*(undefined4 *)0x447c + iVar5 + -1) = 0xff;
      *(int *)0x4476 = iVar5 + -1;
      *(undefined2 *)0x447a = 0xffff;
    }
  }
  FUN_212d_015e();
  return *(undefined2 *)0x447a;
}



void __cdecl16far FUN_2145_00a0(void)

{
  int iVar1;
  undefined2 uVar2;
  int in_AX;
  int iVar3;
  int iVar4;
  undefined2 unaff_DS;
  undefined1 local_4;
  
  if (((*(int *)0x447a != 0) && (*(int *)0x4504 != 0)) && (in_AX != 0)) {
    iVar3 = FUN_2145_000c();
    if (iVar3 == 0) {
      iVar3 = 0;
      if (0 < *(int *)0x4440) {
        iVar1 = *(int *)0x447c;
        iVar4 = *(int *)0x4476;
        uVar2 = *(undefined2 *)0x447e;
        do {
          local_4 = (char)in_AX;
          if ((char)(*(char *)(iVar1 + iVar3) - local_4) == '\x01') {
            *(undefined1 *)(iVar1 + iVar3) = 0;
            iVar4 = iVar4 + 1;
          }
          iVar3 = iVar3 + 1;
        } while (iVar3 < *(int *)0x4440);
        *(int *)0x4476 = iVar4;
      }
      *(undefined1 *)(in_AX * 0x5a + (int)*(undefined4 *)0x44fc) = 0xff;
    }
  }
  return;
}



int __cdecl16far FUN_2145_0106(void)

{
  int in_AX;
  int iVar1;
  int iVar2;
  int iVar3;
  undefined2 uVar4;
  undefined2 unaff_DS;
  int local_a;
  char local_8;
  
  local_a = -1;
  if ((((*(int *)0x447a != 0) && (*(int *)0x4504 != 0)) &&
      (in_AX <= *(int *)0x4476 - *(int *)0x4478)) &&
     ((iVar1 = FUN_2145_000c(), iVar1 == 0 && (iVar1 = FUN_2345_000a(0,0), -1 < iVar1)))) {
    iVar3 = 0;
    for (iVar2 = 0; iVar2 < in_AX; iVar2 = iVar2 + 1) {
      while (*(char *)(*(int *)0x447c + iVar3) != '\0') {
        iVar3 = iVar3 + 1;
        if (*(int *)0x4440 <= iVar3) {
          FUN_2145_00a0();
          return -1;
        }
      }
      local_8 = (char)iVar1;
      *(char *)(*(int *)0x447c + iVar3) = local_8 + '\x01';
      *(int *)0x4476 = *(int *)0x4476 + -1;
    }
    uVar4 = (undefined2)((ulong)*(undefined4 *)0x49c8 >> 0x10);
    iVar3 = (int)*(undefined4 *)0x49c8;
    *(undefined1 *)(iVar1 * 0x5a + iVar3) = 3;
    iVar3 = iVar1 * 0x5a + iVar3;
    *(undefined1 *)(iVar3 + 3) = 0;
    *(undefined1 *)(iVar3 + 2) = 0;
    local_a = iVar1;
  }
  return local_a;
}



int __cdecl16far FUN_2145_01c4(void)

{
  char in_AL;
  int iVar1;
  int in_DX;
  int iVar2;
  undefined2 unaff_DS;
  
  iVar2 = -1;
  if ((*(int *)0x447a != 0) && (iVar1 = FUN_2145_000c(), iVar1 == 0)) {
    iVar2 = in_DX + 1;
    while ((char)(*(char *)(*(int *)0x447c + iVar2) - in_AL) != '\x01') {
      iVar2 = iVar2 + 1;
      if (*(int *)0x4440 <= iVar2) {
        return -1;
      }
    }
  }
  return iVar2;
}



int __cdecl16far FUN_2202_0008(void)

{
  uint uVar1;
  int iVar2;
  undefined2 unaff_DS;
  uint local_4;
  
  iVar2 = 0;
  if ((-1 < *(int *)0x5292) && ((0 < *(int *)0x5292 || (*(int *)0x5290 != 0)))) {
    while (iVar2 == 0) {
      local_4 = *(uint *)0x6028;
      if (((int)*(uint *)0x5292 < 1) && ((0x7fff < *(uint *)0x5292 || (*(uint *)0x5290 < local_4))))
      {
        local_4 = *(uint *)0x5290;
      }
      uVar1 = (*(code *)*(undefined2 *)0x6042)(0x2202,&local_4);
      if (uVar1 == local_4) {
        (*(code *)*(undefined2 *)0x64e8)(0x2202,&local_4);
      }
      else {
        iVar2 = 4;
      }
      if (*(int *)0x5292 < 1) {
        if (*(int *)0x5292 < 0) {
          return iVar2;
        }
        if (*(int *)0x5290 == 0) {
          return iVar2;
        }
      }
    }
  }
  return iVar2;
}



void __cdecl16far FUN_2202_0086(void)

{
  int in_AX;
  int in_DX;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  if (in_AX == 0) {
    local_4 = 0x1000;
    if (*(int *)0x448c == 1) {
      (*(code *)*(undefined2 *)0x4496)(0x2202,&local_4);
      return;
    }
    (*(code *)*(undefined2 *)0x448e)(0x2202,&local_4);
    return;
  }
  if (in_AX == 1) {
    if (*(int *)0x448c == 1) {
      if (in_DX == 1) {
        (*(code *)*(undefined2 *)0x449e)
                  (0x2202,*(undefined2 *)0x4ea4,*(undefined2 *)0x4ea6,*(undefined2 *)0x5a9e,
                   *(undefined2 *)0x5aa0,*(undefined2 *)0x6042,*(undefined2 *)0x6044);
        return;
      }
      if (in_DX != 2) {
        (*(code *)*(undefined2 *)0x449a)
                  (0x2202,*(undefined2 *)0x4ea4,*(undefined2 *)0x4ea6,*(undefined2 *)0x64e8,
                   *(undefined2 *)0x64ea,*(undefined2 *)0x6042,*(undefined2 *)0x6044);
        return;
      }
      (*(code *)*(undefined2 *)0x44a2)
                (0x2202,*(undefined2 *)0x4ea4,*(undefined2 *)0x4ea6,*(undefined2 *)0x5a9e,
                 *(undefined2 *)0x5aa0,*(undefined2 *)0x6348,*(undefined2 *)0x634a);
      return;
    }
    (*(code *)*(undefined2 *)0x4492)
              (0x2202,*(undefined2 *)0x4ea4,*(undefined2 *)0x4ea6,*(undefined2 *)0x64e8,
               *(undefined2 *)0x64ea,*(undefined2 *)0x6042,*(undefined2 *)0x6044);
  }
  else {
    FUN_2202_0008();
  }
  return;
}



int __stdcall16far FUN_221a_0000(undefined2 param_1,undefined2 param_2,undefined2 *param_3)

{
  uint *puVar1;
  uint uVar2;
  undefined2 uVar3;
  uint uVar4;
  uint uVar5;
  undefined1 uVar6;
  int in_AX;
  undefined2 uVar7;
  int iVar8;
  undefined2 in_BX;
  undefined2 *puVar9;
  uint *puVar10;
  char *pcVar11;
  undefined2 unaff_DS;
  bool bVar12;
  long lVar13;
  undefined1 local_20;
  uint local_1a;
  int local_18;
  int local_16;
  undefined2 local_14;
  char *local_12;
  undefined2 local_10;
  undefined2 *local_a;
  undefined2 local_8;
  int local_6;
  uint local_4;
  
  local_6 = -1;
  local_16 = -1;
  FUN_2388_0d58(0x44e8);
  *param_3 = 0;
  uVar7 = FUN_2388_0a6a(in_BX,0x72);
  iVar8 = FUN_2388_09e8(uVar7);
  local_4 = (uint)(iVar8 != 0);
  if (((local_4 != 0) && (in_AX != 0)) && (*(int *)0x44e6 == 0)) {
    local_16 = FUN_2345_000a(param_1,param_2);
  }
  if (local_16 < 0) {
    *(undefined1 *)((undefined2 *)param_3 + 2) = 0;
    ((undefined2 *)param_3)[4] = 0xffff;
    iVar8 = FUN_1297_0104(param_1,param_2);
    ((undefined2 *)param_3)[3] = iVar8;
    if (iVar8 == 0) goto LAB_221a_0310;
    ((undefined2 *)param_3)[1] = local_4;
    ((undefined2 *)param_3)[0xc] = 0;
    if (local_4 == 0) {
      ((undefined2 *)param_3)[0x14] = 0;
      local_20 = (undefined1)in_AX;
      *(undefined1 *)((undefined2 *)param_3 + 0x15) = local_20;
      FUN_2388_0dec((undefined2 *)param_3 + 0xd,param_3._2_2_,0x44b4);
      lVar13 = FUN_1bea_0008(1,0,(undefined2 *)param_3 + 0xd,param_3._2_2_);
      if (lVar13 == 0) goto LAB_221a_0310;
      ((undefined2 *)param_3)[0xb] = 0;
      ((undefined2 *)param_3)[10] = 0;
    }
    else {
      FUN_2388_0762(((undefined2 *)param_3)[3],&local_1a);
      lVar13 = FUN_1bca_0000(1,0,(undefined2 *)param_3 + 0xd,param_3._2_2_);
      if (((lVar13 == 0) ||
          (iVar8 = FUN_2388_0d1c((undefined2 *)param_3 + 0xd,param_3._2_2_,0x44a6), iVar8 != 0)) ||
         (lVar13 = FUN_1bca_0000(1,0,(undefined2 *)param_3 + 0x15,param_3._2_2_), lVar13 == 0))
      goto LAB_221a_0310;
      bVar12 = 0xff4f < local_1a;
      local_1a = local_1a + 0xb0;
      local_18 = local_18 + (uint)bVar12;
      FUN_2388_087e(((undefined2 *)param_3)[3],&local_1a);
      ((undefined2 *)param_3)[0xb] = 0;
      ((undefined2 *)param_3)[10] = 0;
      local_14 = 0;
      if (0 < (int)((undefined2 *)param_3)[0x14]) {
        iVar8 = ((undefined2 *)param_3)[0x14];
        puVar10 = (undefined2 *)param_3 + 0x16;
        do {
          uVar4 = *puVar10;
          uVar5 = puVar10[1];
          puVar1 = (undefined2 *)param_3 + 10;
          uVar2 = *puVar1;
          *puVar1 = *puVar1 + uVar4;
          ((undefined2 *)param_3)[0xb] =
               ((undefined2 *)param_3)[0xb] + uVar5 + (uint)CARRY2(uVar2,uVar4);
          iVar8 = iVar8 + -1;
          puVar10 = puVar10 + 5;
        } while (iVar8 != 0);
        unaff_DS = 0x25e7;
      }
    }
    puVar1 = (uint *)0x44ca;
    uVar2 = *puVar1;
    *puVar1 = *puVar1 + 1;
    *(int *)0x44cc = *(int *)0x44cc + (uint)(0xfffe < uVar2);
  }
  else {
    pcVar11 = (char *)*(undefined2 *)0x4b56;
    uVar7 = *(undefined2 *)0x4b58;
    if (*pcVar11 == '\x03') {
      uVar6 = 1;
    }
    else {
      uVar6 = 2;
    }
    *(undefined1 *)((undefined2 *)param_3 + 2) = uVar6;
    ((undefined2 *)param_3)[5] = *(undefined2 *)(pcVar11 + 0x12);
    ((undefined2 *)param_3)[7] = 0;
    ((undefined2 *)param_3)[6] = 0;
    ((undefined2 *)param_3)[4] = local_16;
    ((undefined2 *)param_3)[9] = 0x4000;
    uVar3 = *(undefined2 *)(pcVar11 + 0x16);
    ((undefined2 *)param_3)[10] = *(undefined2 *)(pcVar11 + 0x14);
    ((undefined2 *)param_3)[0xb] = uVar3;
    iVar8 = *(int *)(pcVar11 + 0x18);
    ((undefined2 *)param_3)[0x14] = iVar8;
    ((undefined2 *)param_3)[8] = 0xffff;
    ((undefined2 *)param_3)[1] = 0xffff;
    ((undefined2 *)param_3)[0xc] = 0;
    local_14 = 0;
    if (0 < iVar8) {
      local_a = (undefined2 *)param_3 + 0x15;
      local_8 = param_3._2_2_;
      local_12 = pcVar11 + 0x1a;
      local_10 = *(undefined2 *)0x4b58;
      iVar8 = 0;
      puVar9 = local_a;
      pcVar11 = local_12;
      do {
        *(undefined1 *)puVar9 = 0;
        uVar7 = *(undefined2 *)pcVar11;
        uVar3 = *(undefined2 *)(pcVar11 + 2);
        puVar9[1] = uVar7;
        puVar9[2] = uVar3;
        puVar9[3] = uVar7;
        puVar9[4] = uVar3;
        puVar9 = puVar9 + 5;
        pcVar11 = pcVar11 + 4;
        iVar8 = iVar8 + 1;
      } while (iVar8 < (int)((undefined2 *)param_3)[0x14]);
    }
    if (*(char *)*(undefined4 *)0x4b56 == '\x03') {
      puVar1 = (uint *)0x44c2;
      uVar2 = *puVar1;
      *puVar1 = *puVar1 + 1;
      *(int *)0x44c4 = *(int *)0x44c4 + (uint)(0xfffe < uVar2);
    }
    else {
      puVar1 = (uint *)0x44c6;
      uVar2 = *puVar1;
      *puVar1 = *puVar1 + 1;
      *(int *)0x44c8 = *(int *)0x44c8 + (uint)(0xfffe < uVar2);
    }
  }
  *param_3 = 0xffff;
  local_6 = 0;
LAB_221a_0310:
  if (((local_6 != 0) && (local_16 == 0)) && (((undefined2 *)param_3)[3] != 0)) {
    FUN_2388_02c2(((undefined2 *)param_3)[3]);
  }
  return local_6;
}



void __stdcall16far FUN_221a_033c(undefined4 param_1)

{
  undefined1 in_AL;
  
  *(undefined1 *)((int)param_1 + 0x2b) = in_AL;
  return;
}



undefined2 __stdcall16far FUN_221a_034e(int *param_1)

{
  int *piVar1;
  undefined2 uVar2;
  undefined2 uVar3;
  long lVar4;
  
  uVar3 = (undefined2)((ulong)param_1 >> 0x10);
  piVar1 = (int *)param_1;
  uVar2 = 0;
  if (*param_1 != 0) {
    if (((char)piVar1[2] == '\x01') || ((char)piVar1[2] == '\x02')) {
      piVar1[8] = -1;
      piVar1[9] = 0x4000;
      piVar1[7] = 0;
      piVar1[6] = 0;
    }
    else {
      if (piVar1[1] == 0) {
        FUN_2388_0898(piVar1[3]);
        lVar4 = FUN_1bea_0008(1,0,piVar1 + 0xd,uVar3);
        if (lVar4 == 0) {
          uVar2 = 1;
        }
        else {
          uVar2 = 0;
        }
      }
      FUN_2388_02c2(piVar1[3]);
    }
  }
  *param_1 = 0;
  return uVar2;
}



ulong __stdcall16far
FUN_2258_0002(undefined4 param_1,int param_2,int param_3,undefined2 param_4,undefined2 param_5)

{
  uint *puVar1;
  byte bVar2;
  int in_AX;
  uint uVar3;
  int iVar4;
  int in_DX;
  int iVar5;
  undefined2 uVar6;
  undefined2 unaff_DS;
  long lVar7;
  long lVar8;
  ulong uVar9;
  int local_1e;
  int local_1c;
  uint local_1a;
  int local_18;
  int local_16;
  undefined4 local_10;
  int local_c;
  uint local_a;
  int local_8;
  undefined4 local_6;
  
  local_1c = 0;
  local_1e = 0;
  local_16 = 0;
  if (in_DX == 0 && in_AX == 0) {
    uVar3 = 0;
    goto LAB_2258_001e;
  }
  if ((param_2 != 1) || (local_6 = CONCAT22(in_DX,in_AX), param_3 != 0)) {
    local_6 = FUN_2388_0bbc(param_2,param_3,in_AX,in_DX);
  }
  uVar6 = (undefined2)((ulong)param_1 >> 0x10);
  iVar5 = (int)param_1;
  iVar4 = *(int *)(iVar5 + 0x18);
  *(int *)(iVar5 + 0x18) = *(int *)(iVar5 + 0x18) + 1;
  if (*(char *)(iVar5 + 4) == '\x01') {
    local_10._2_2_ = 0;
    local_10._0_2_ = 0;
    iVar4 = FUN_22ec_0008(local_6,param_4,param_5,iVar5 + 0x12,uVar6,iVar5 + 0x10,uVar6);
    lVar8 = CONCAT22(local_10._2_2_,(undefined2)local_10);
    if (iVar4 == 0) {
      lVar8 = local_6;
    }
  }
  else if (*(char *)(iVar5 + 4) == '\x02') {
    local_10._2_2_ = 0;
    local_10._0_2_ = 0;
    iVar4 = FUN_2343_0004(local_6,*(undefined2 *)(iVar5 + 10),*(undefined2 *)(iVar5 + 0xc),
                          *(undefined2 *)(iVar5 + 0xe),0,param_4,param_5);
    lVar8 = CONCAT22(local_10._2_2_,(undefined2)local_10);
    if (iVar4 == 0) {
      puVar1 = (uint *)(iVar5 + 0xc);
      uVar3 = *puVar1;
      *puVar1 = *puVar1 + (uint)local_6;
      *(int *)(iVar5 + 0xe) =
           *(int *)(iVar5 + 0xe) + local_6._2_2_ + (uint)CARRY2(uVar3,(uint)local_6);
      lVar8 = local_6;
    }
  }
  else {
    local_10._2_2_ = 0;
    local_10._0_2_ = 0;
    lVar8 = 0;
    iVar4 = iVar5 + iVar4 * 10;
    bVar2 = *(byte *)(iVar4 + 0x2a);
    *(uint *)0x448c = (uint)bVar2;
    local_a = *(uint *)(iVar4 + 0x30);
    iVar4 = *(int *)(iVar4 + 0x32);
    local_c = (-(uint)(bVar2 == 0) & 1) + 1;
    local_8 = iVar4;
    if (local_c == 1) {
      local_1e = FUN_1cc9_02e2();
      lVar8 = CONCAT22(local_10._2_2_,(undefined2)local_10);
      local_1c = iVar4;
      if (iVar4 != 0 || local_1e != 0) {
        lVar7 = FUN_1bca_0000(1,0,local_1e,iVar4);
        lVar8 = CONCAT22(local_10._2_2_,(undefined2)local_10);
        if (lVar7 == 0) goto LAB_2258_01f2;
        lVar8 = FUN_2309_004a(param_4,param_5,local_1e,iVar4,local_6);
        local_16 = -1;
      }
    }
    if (local_16 == 0) {
      local_10 = lVar8;
      FUN_2388_0762(*(undefined2 *)(iVar5 + 6),&local_1a);
      lVar8 = FUN_2309_004a(param_4,param_5,*(undefined2 *)(iVar5 + 6),unaff_DS,local_6);
      if (local_c == 1) {
        local_10 = lVar8;
        FUN_2388_07fe(*(undefined2 *)(iVar5 + 6),local_a + local_1a,
                      local_8 + local_18 + (uint)CARRY2(local_a,local_1a),0);
        lVar8 = local_10;
      }
    }
  }
LAB_2258_01f2:
  local_10 = lVar8;
  if (local_1c != 0 || local_1e != 0) {
    FUN_1cc9_0310(local_1e,local_1c);
  }
  if (local_10 != CONCAT22(in_DX,in_AX)) {
    uVar9 = FUN_2388_0b22(local_10,in_AX,in_DX);
    return uVar9;
  }
  uVar3 = 1;
LAB_2258_001e:
  return (ulong)uVar3;
}



void __stdcall16far FUN_227b_0000(undefined2 param_1,undefined2 param_2)

{
  code *pcVar1;
  int in_AX;
  
  FUN_2388_0dd4(param_1,param_2);
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  if (in_AX != 0) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
  }
  return;
}



int __cdecl16far FUN_227e_000c(void)

{
  char cVar1;
  int extraout_DX;
  char in_BL;
  undefined2 unaff_DS;
  
  if (*(int *)0x3c68 != 0) {
    cVar1 = (*(code *)*(undefined2 *)0x3c6c)(0x227e);
    if ((cVar1 == '\0') && (in_BL == -0x50)) {
      return extraout_DX << 4;
    }
  }
  return 0;
}



undefined2 __cdecl16far FUN_2281_0008(void)

{
  int iVar1;
  undefined2 uVar2;
  int iVar3;
  undefined2 unaff_DS;
  
  uVar2 = 0;
  iVar3 = *(int *)0x44f6;
  if (iVar3 < 0x10) {
    uVar2 = (*(code *)*(undefined2 *)0x3c6c)(0x2281);
    if ((char)uVar2 != '\0') {
      uVar2 = 0;
      iVar1 = *(int *)0x44f6;
      *(int *)0x44f6 = *(int *)0x44f6 + 1;
      *(int *)(iVar1 * 4 + 0x5aac) = iVar3;
    }
  }
  return uVar2;
}



void __cdecl16far FUN_2281_004a(undefined2 param_1,int param_2)

{
  int iVar1;
  int *piVar2;
  undefined2 unaff_DS;
  
  piVar2 = (int *)0x5aac;
  for (iVar1 = *(int *)0x44f6; iVar1 != 0; iVar1 = iVar1 + -1) {
    if (*piVar2 == param_2) goto joined_r0x00022877;
    piVar2 = piVar2 + 1;
  }
LAB_2281_0077:
  (*(code *)*(undefined2 *)0x3c6c)(0x2281);
  return;
joined_r0x00022877:
  while (iVar1 = iVar1 + -1, iVar1 != 0) {
    *piVar2 = piVar2[1];
    piVar2 = piVar2 + 1;
  }
  *(int *)0x44f6 = *(int *)0x44f6 + -1;
  goto LAB_2281_0077;
}



void __cdecl16far FUN_2289_0000(void)

{
  int iVar1;
  int iVar2;
  undefined2 unaff_DS;
  
  iVar2 = 0x5aac;
  for (iVar1 = *(int *)0x44f6; iVar1 != 0; iVar1 = iVar1 + -1) {
    (*(code *)*(undefined2 *)0x3c6c)(0x2289,iVar2);
    iVar2 = iVar2 + 2;
  }
  *(undefined2 *)0x44f6 = 0;
  return;
}



undefined2 __cdecl16far FUN_228b_0004(void)

{
  return 0xffff;
}



void __cdecl16far FUN_228b_0040(void)

{
  undefined2 unaff_DS;
  
  if (*(int *)0x3c68 != 0) {
    (*(code *)*(undefined2 *)0x3c6c)(0x228b);
  }
  return;
}



bool __cdecl16far FUN_2290_0006(void)

{
  int iVar1;
  undefined2 unaff_DS;
  bool bVar2;
  
  bVar2 = false;
  if (*(char *)0x44fa == '\x03') {
    iVar1 = FUN_2145_000c();
    *(uint *)0x4502 = (uint)(iVar1 == 0);
    bVar2 = (iVar1 == 0) == 0;
  }
  return bVar2;
}



undefined2 __cdecl16far FUN_2292_000a(void)

{
  undefined2 uVar1;
  int in_AX;
  int iVar2;
  undefined2 unaff_DS;
  
  if (*(char *)0x44fa == '\x03') {
    iVar2 = FUN_2290_0006();
    if (iVar2 != 0) {
      return 0xffff;
    }
    uVar1 = *(undefined2 *)0x44fe;
    *(int *)0x4b56 = in_AX * 0x5a + *(int *)0x44fc;
    *(undefined2 *)0x4b58 = uVar1;
  }
  else {
    iVar2 = FUN_2343_0004(0x5a,0,*(undefined2 *)0x4500,(long)in_AX * 0x5a,0,0x648e);
    if (iVar2 != 0) {
      return 0xffff;
    }
  }
  return 0;
}



int __cdecl16far FUN_2297_000e(void)

{
  undefined2 uVar1;
  undefined2 uVar2;
  int iVar3;
  undefined1 *puVar4;
  undefined2 unaff_DS;
  long lVar5;
  int local_12;
  undefined2 local_c;
  
  local_12 = -1;
  local_c = 0;
  lVar5 = 0;
  if (*(int *)0x447a == 0) {
    if (*(char *)0x44f9 != '\0') goto LAB_2297_00f6;
    *(undefined1 *)0x44fa = 4;
    local_c = 0x34bc;
    iVar3 = FUN_228b_0004();
    *(int *)0x4500 = iVar3;
    if (iVar3 < 1) goto LAB_2297_00f6;
    lVar5 = FUN_1cc9_0136(0x4506,unaff_DS);
    *(undefined2 *)0x49c8 = (int)lVar5;
    *(undefined2 *)0x49ca = (int)((ulong)lVar5 >> 0x10);
    if (lVar5 == 0) goto LAB_2297_00f6;
    *(undefined2 *)0x4b56 = 0x648e;
    *(undefined2 *)0x4b58 = unaff_DS;
  }
  else {
    *(undefined1 *)0x44fa = 3;
    uVar1 = *(undefined2 *)0x4462;
    uVar2 = CONCAT11((char)((uint)*(undefined2 *)0x4460 >> 8) + '\b',(char)*(undefined2 *)0x4460);
    *(undefined2 *)0x44fc = uVar2;
    *(undefined2 *)0x44fe = uVar1;
    *(undefined2 *)0x49c8 = uVar2;
    *(undefined2 *)0x49ca = uVar1;
    iVar3 = FUN_2290_0006();
    if (iVar3 != 0) goto LAB_2297_00f6;
  }
  uVar1 = *(undefined2 *)0x49ca;
  iVar3 = 0x96;
  puVar4 = (undefined1 *)*(undefined2 *)0x49c8;
  do {
    *puVar4 = 0xff;
    iVar3 = iVar3 + -1;
    puVar4 = puVar4 + 0x5a;
  } while (iVar3 != 0);
  if ((*(char *)0x44fa != '\x04') ||
     (iVar3 = FUN_2343_0004(local_c,0,0,lVar5,*(undefined2 *)0x4500,0,0), iVar3 == 0)) {
    *(undefined2 *)0x4504 = 0xffff;
    local_12 = 0;
  }
LAB_2297_00f6:
  if (lVar5 != 0) {
    FUN_1cc9_0310(lVar5);
  }
  if ((local_12 != 0) && (*(undefined1 *)0x44fa = 0xff, 0 < *(int *)0x4500)) {
    FUN_228b_0040();
    *(undefined2 *)0x4500 = 0xffff;
  }
  return local_12;
}



int __cdecl16far FUN_22aa_001b(void)

{
  char *pcVar1;
  int iVar2;
  int iVar3;
  char *pcVar4;
  undefined2 unaff_DS;
  
  iVar3 = 0;
  pcVar4 = (char *)*(undefined2 *)0x45a2;
  iVar2 = (int)pcVar4 - (int)&stack0xfffe;
  if (pcVar4 < &stack0xfffe) {
    iVar3 = -iVar2;
    do {
      pcVar1 = pcVar4;
      pcVar4 = pcVar4 + 1;
      if (*pcVar1 != '\0') break;
      iVar3 = iVar3 + -1;
    } while (iVar3 != 0);
    iVar3 = -iVar2 - iVar3;
  }
  return iVar3;
}



void __cdecl16far FUN_22b0_0000(void)

{
  char cVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  char cVar5;
  int in_BX;
  int iVar6;
  undefined2 unaff_DS;
  char *local_14;
  int local_a;
  int local_8;
  
  iVar6 = 0;
  iVar3 = 0;
  local_8 = 0;
  do {
    cVar1 = (-(iVar3 == 0) & 0xebU) + 0x3f;
    local_a = 0;
    do {
      iVar4 = 0;
      do {
        iVar2 = 0;
        do {
          cVar5 = (char)iVar6;
          if (local_a == 0) {
            local_14 = (char *)((local_8 + iVar2) * 3 + in_BX);
            *local_14 = cVar5;
          }
          else {
            local_14 = (char *)((local_8 + iVar2) * 3 + in_BX);
            *local_14 = cVar1;
          }
          if (iVar4 == 0) {
            local_14[1] = cVar5;
          }
          else {
            local_14[1] = cVar1;
          }
          if (iVar2 == 0) {
            local_14[2] = cVar5;
          }
          else {
            local_14[2] = cVar1;
          }
          iVar2 = iVar2 + 1;
        } while (iVar2 < 2);
        local_8 = local_8 + 2;
        iVar4 = iVar4 + 1;
      } while (iVar4 < 2);
      local_a = local_a + 1;
    } while (local_a < 2);
    iVar3 = iVar3 + 1;
    iVar6 = iVar6 + 0x15;
  } while (iVar6 < 0x2a);
  *(undefined1 *)(in_BX + 0x13) = 0x15;
  return;
}



void __cdecl16far FUN_22b0_00ca(void)

{
  undefined1 uVar1;
  int iVar2;
  undefined1 *in_BX;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined1 local_6 [4];
  
  local_6[0] = 0;
  local_6[1] = 0x15;
  local_6[2] = 0x2a;
  local_6[3] = 0x3f;
  iVar2 = 0;
  do {
    uVar1 = local_6[iVar2];
    *in_BX = uVar1;
    in_BX[1] = uVar1;
    in_BX[2] = uVar1;
    in_BX = in_BX + 3;
    iVar2 = iVar2 + 1;
  } while (iVar2 < 4);
  return;
}



void __stdcall16far FUN_22b0_0100(int param_1,int param_2)

{
  int in_AX;
  undefined1 uVar1;
  int in_DX;
  int in_BX;
  undefined1 *puVar2;
  undefined2 unaff_DS;
  int local_a;
  uint local_6;
  int local_4;
  
  local_6 = 0;
  local_4 = param_2;
  if (0 < in_DX) {
    puVar2 = (undefined1 *)(in_AX * 3 + in_BX);
    local_a = in_DX;
    do {
      uVar1 = (undefined1)local_4;
      *puVar2 = uVar1;
      puVar2[1] = uVar1;
      puVar2[2] = uVar1;
      if (1 < in_DX) {
        for (local_6 = local_6 - (param_2 - param_1); in_DX - 1U <= local_6;
            local_6 = local_6 + (1 - in_DX)) {
          local_4 = local_4 + 1;
        }
      }
      puVar2 = puVar2 + 3;
      local_a = local_a + -1;
    } while (local_a != 0);
  }
  return;
}



void __stdcall16far FUN_22c7_000e(uint *param_1,undefined4 param_2)

{
  uint uVar1;
  uint uVar2;
  undefined2 uVar3;
  bool bVar4;
  int in_AX;
  int iVar5;
  uint *puVar6;
  uint *puVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  undefined2 uVar12;
  undefined2 uVar13;
  int local_10;
  int local_e;
  int local_c;
  int local_a;
  int local_4;
  
  iVar5 = in_AX + -1;
  do {
    local_4 = 0;
    local_e = 0;
    if (0 < iVar5) {
      local_10 = (in_AX + -1) * 2;
      local_c = 0;
      local_a = 0;
      do {
        if (local_4 != 0) break;
        uVar12 = (undefined2)((ulong)param_1 >> 0x10);
        puVar6 = (uint *)param_1;
        puVar7 = (uint *)((int)puVar6 + local_a);
        if (((int)puVar7[3] <= (int)puVar7[1]) &&
           (((int)puVar7[3] < (int)puVar7[1] || (puVar7[2] < *puVar7)))) {
          uVar1 = *puVar7;
          uVar2 = puVar7[1];
          uVar13 = (undefined2)((ulong)param_2 >> 0x10);
          iVar8 = (int)param_2;
          uVar3 = *(undefined2 *)(iVar8 + local_c);
          if (0 < local_10) {
            FUN_2388_0eb0(puVar7,uVar12,puVar7 + 2,uVar12,local_10 << 1);
            FUN_2388_0eb0(iVar8 + local_c,uVar13,iVar8 + local_c + 2,uVar13,local_10);
          }
          iVar11 = 0;
          if (0 < iVar5) {
            bVar4 = false;
            puVar7 = puVar6;
            do {
              if (bVar4) break;
              if (((int)uVar2 <= (int)puVar7[1]) &&
                 (((int)uVar2 < (int)puVar7[1] || (uVar1 < *puVar7)))) {
                bVar4 = true;
              }
              iVar11 = iVar11 + 1;
              puVar7 = puVar7 + 2;
            } while (iVar11 < iVar5);
          }
          local_4 = -1;
          iVar9 = (in_AX - iVar11) + -1;
          iVar10 = iVar9 * 2;
          if (0 < iVar10) {
            FUN_2388_0eb0(puVar6 + iVar11 * 2 + 2,uVar12,puVar6 + iVar11 * 2,uVar12,iVar9 * 4);
            iVar9 = iVar11 * 2 + iVar8;
            FUN_2388_0eb0(iVar9 + 2,uVar13,iVar9,uVar13,iVar10);
          }
          puVar6[iVar11 * 2] = uVar1;
          puVar6[iVar11 * 2 + 1] = uVar2;
          *(undefined2 *)(iVar11 * 2 + iVar8) = uVar3;
        }
        local_c = local_c + 2;
        local_10 = local_10 + -2;
        local_a = local_a + 4;
        local_e = local_e + 1;
      } while (local_e < iVar5);
    }
    if (local_4 == 0) {
      return;
    }
  } while( true );
}



void __stdcall16far FUN_22e0_0008(undefined4 param_1,undefined4 param_2)

{
  undefined1 *puVar1;
  byte bVar2;
  undefined1 uVar3;
  int in_AX;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  undefined1 *puVar8;
  int iVar9;
  int iVar10;
  undefined1 *puVar11;
  undefined2 uVar12;
  undefined2 uVar13;
  int local_12;
  
  local_12 = 0;
  uVar12 = (undefined2)((ulong)param_1 >> 0x10);
  iVar10 = (int)param_1;
  do {
    iVar7 = local_12;
    while( true ) {
      if (in_AX + -1 <= local_12) {
        return;
      }
      bVar2 = *(byte *)(iVar7 + iVar10 + 1);
      if (bVar2 < *(byte *)(iVar7 + iVar10)) break;
      local_12 = local_12 + 1;
      iVar7 = iVar7 + 1;
    }
    iVar4 = ((in_AX + -1) - local_12) + -1;
    puVar11 = (undefined1 *)(iVar10 + iVar7);
    puVar8 = puVar11 + 2;
    iVar7 = iVar4;
    if (iVar4 != 0) {
      for (; puVar11 = puVar11 + 1, iVar7 != 0; iVar7 = iVar7 + -1) {
        puVar1 = puVar8;
        puVar8 = puVar8 + 1;
        *puVar11 = *puVar1;
      }
    }
    uVar13 = (undefined2)((ulong)param_2 >> 0x10);
    iVar7 = (int)param_2;
    puVar11 = (undefined1 *)(iVar7 + local_12 + 1);
    puVar8 = (undefined1 *)(iVar7 + local_12 + 1);
    uVar3 = *puVar8;
    if (iVar4 != 0) {
      for (; puVar11 = puVar11 + 1, iVar4 != 0; iVar4 = iVar4 + -1) {
        puVar1 = puVar8;
        puVar8 = puVar8 + 1;
        *puVar1 = *puVar11;
      }
    }
    iVar9 = 0;
    for (iVar4 = 0; (iVar9 < in_AX + -1 && (*(byte *)(iVar4 + iVar10) < bVar2)); iVar4 = iVar4 + 1)
    {
      iVar9 = iVar9 + 1;
    }
    iVar5 = (in_AX + -1) - iVar9;
    if (iVar5 != 0) {
      puVar11 = (undefined1 *)(iVar10 + iVar4 + iVar5);
      puVar8 = puVar11;
      for (iVar6 = iVar5; puVar8 = puVar8 + -1, iVar6 != 0; iVar6 = iVar6 + -1) {
        puVar1 = puVar11;
        puVar11 = puVar11 + -1;
        *puVar1 = *puVar8;
      }
      puVar11 = (undefined1 *)(iVar9 + iVar7 + iVar5);
      puVar8 = puVar11;
      for (; puVar8 = puVar8 + -1, iVar5 != 0; iVar5 = iVar5 + -1) {
        puVar1 = puVar11;
        puVar11 = puVar11 + -1;
        *puVar1 = *puVar8;
      }
    }
    *(undefined1 *)(iVar9 + iVar7) = uVar3;
    *(byte *)(iVar4 + iVar10) = bVar2;
  } while( true );
}



undefined2 __stdcall16far
FUN_22ec_0008(uint param_1,uint param_2,int param_3,undefined2 param_4,int *param_5,int *param_6)

{
  int iVar1;
  uint uVar2;
  undefined2 unaff_DS;
  bool bVar3;
  undefined4 uVar4;
  undefined2 local_6;
  
  local_6 = 0xffff;
  uVar4 = CONCAT22(param_4,param_3);
  if ((*param_6 < 0) ||
     (iVar1 = FUN_212d_00d4(2,*param_6), uVar4 = CONCAT22(param_4,param_3), iVar1 == 0)) {
    while( true ) {
      param_4 = (undefined2)((ulong)uVar4 >> 0x10);
      param_3 = (int)uVar4;
      if (((int)param_2 < 0) || (((int)param_2 < 1 && (param_1 == 0)))) break;
      if (0x3fff < *param_5) {
        iVar1 = FUN_2145_01c4();
        *param_6 = iVar1;
        if (iVar1 < 0) {
          return 0xffff;
        }
        iVar1 = FUN_212d_00d4(2,*param_6);
        if (iVar1 != 0) {
          return 0xffff;
        }
        *param_5 = 0;
      }
      uVar2 = 0x4000 - *param_5;
      if (((int)param_2 < 1) && ((0x7fff < param_2 || (param_1 < uVar2)))) {
        uVar2 = param_1;
      }
      if (uVar2 != 0) {
        FUN_2388_0c4a(uVar4,*param_5 + *(int *)0x4464,*(undefined2 *)0x4466,uVar2);
        param_3 = param_3 + uVar2;
        *param_5 = *param_5 + uVar2;
        bVar3 = param_1 < uVar2;
        param_1 = param_1 - uVar2;
        param_2 = param_2 - bVar3;
      }
      uVar4 = FUN_2357_000e(param_3,param_4);
    }
    local_6 = 0;
  }
  return local_6;
}



undefined2 __cdecl16far FUN_22fb_0006(void)

{
  int iVar1;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined2 auStack_10 [4];
  int local_8;
  undefined2 local_6;
  undefined2 local_4;
  
  local_6 = 0xffff;
  local_4 = 0xffff;
  if (*(int *)0x447a != 0) {
    local_8 = 0;
    do {
      local_4 = FUN_2145_01c4();
      auStack_10[local_8] = local_4;
      local_8 = local_8 + 1;
    } while (local_8 < 4);
    for (local_8 = 0; local_8 < 4; local_8 = local_8 + 1) {
      iVar1 = FUN_212d_00d4(local_8,auStack_10[local_8]);
      if (iVar1 != 0) {
        return local_6;
      }
    }
    local_6 = 0;
  }
  return local_6;
}



void __cdecl16far FUN_2302_000a(void)

{
  undefined2 uVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined2 local_8;
  
  local_8 = 0;
  do {
    uVar1 = FUN_2145_01c4();
    uVar2 = FUN_2145_01c4();
    FUN_212d_00d4(2,uVar1);
    FUN_212d_00d4(3,uVar2);
    FUN_2388_0c4a(*(undefined2 *)0x4468,*(undefined2 *)0x446a,*(undefined2 *)0x4464,
                  *(undefined2 *)0x4466,0x4000);
    local_8 = local_8 + 1;
  } while (local_8 < 4);
  return;
}



undefined2 __cdecl16far FUN_2309_000a(undefined2 param_1)

{
  return param_1;
}



void __cdecl16far FUN_2309_0014(void)

{
  undefined2 unaff_DS;
  
  if ((int)((ulong)*(undefined4 *)0x4516 >> 0x10) != 0 || (int)*(undefined4 *)0x4516 != 0) {
    (*(code *)*(undefined2 *)0x4516)(0x2309);
  }
  return;
}



void __cdecl16far
FUN_2309_002a(undefined2 param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4)

{
  undefined2 unaff_DS;
  
  *(undefined2 *)0x4512 = param_1;
  *(undefined2 *)0x4514 = param_2;
  *(undefined2 *)0x4516 = param_3;
  *(undefined2 *)0x4518 = param_4;
  return;
}



undefined4 __stdcall16far
FUN_2309_004a(undefined2 *param_1,undefined2 param_2,undefined2 param_3,undefined2 param_4,
             undefined2 param_5,undefined2 param_6)

{
  int in_AX;
  undefined2 uVar1;
  int iVar2;
  int in_DX;
  int in_BX;
  undefined2 unaff_DS;
  bool bVar3;
  undefined4 uVar4;
  int *local_8;
  int local_6;
  undefined2 *local_4;
  
  local_6 = 0;
  if (in_DX == 0) {
    *(undefined2 *)0x6042 = 6;
    *(undefined2 *)0x6044 = 0x2359;
    *(undefined2 *)0x6348 = param_3;
    *(undefined2 *)0x634a = param_4;
  }
  else {
    *(undefined2 *)0x6042 = 8;
    *(undefined2 *)0x6044 = 0x2367;
    uVar1 = FUN_2309_000a(param_3,param_4);
    *(undefined2 *)0x4e7c = uVar1;
  }
  if (in_BX == 2) {
    *(undefined2 *)0x64e8 = 4;
    *(undefined2 *)0x64ea = 0x2378;
    *(undefined2 *)0x6978 = *param_1;
    *(undefined2 *)0x4bb4 = param_1[1];
    *(undefined2 *)0x4e82 = param_1[2];
  }
  else if (in_BX == 0) {
    *(undefined2 *)0x64e8 = 10;
    *(undefined2 *)0x64ea = 0x2360;
    *(undefined2 *)0x5a9e = param_1;
    *(undefined2 *)0x5aa0 = param_2;
  }
  else {
    *(undefined2 *)0x64e8 = 2;
    *(undefined2 *)0x64ea = 0x236f;
    uVar1 = FUN_2309_000a(param_1,param_2);
    *(undefined2 *)0x4e7a = uVar1;
  }
  *(undefined2 *)0x6986 = 0;
  *(undefined2 *)0x6984 = 0;
  *(undefined2 *)0x4b0c = 0;
  *(undefined2 *)0x4b0a = 0;
  if (in_AX == 0) {
    if (*(int *)0x448c == 1) {
      *(undefined2 *)0x6028 = 0xd1d0;
      bVar3 = *(int *)0x4498 == 0 && *(int *)0x4496 == 0;
    }
    else {
      *(undefined2 *)0x6028 = 0x89b8;
      bVar3 = *(int *)0x4490 == 0 && *(int *)0x448e == 0;
    }
    if (bVar3) {
      FUN_1ed0_03d6(*(int *)0x448c,*(int *)0x448c >> 0xf,0,0);
    }
    *(undefined2 *)0x5290 = param_5;
    *(undefined2 *)0x5292 = param_6;
    *(undefined2 *)0x4a84 = 0xffff;
    *(undefined2 *)0x4a86 = 0xffff;
    local_8 = (int *)0x5290;
    local_4 = (undefined2 *)0x4b0a;
  }
  else if (in_AX == 1) {
    *(undefined2 *)0x5290 = 0xffff;
    *(undefined2 *)0x5292 = 0xffff;
    *(undefined2 *)0x4a84 = param_5;
    *(undefined2 *)0x4a86 = param_6;
    local_8 = (int *)0x4a84;
    if (*(int *)0x448c == 1) {
      if (((in_DX == 0) && (in_BX == 0)) && (*(int *)0x44a4 != 0 || *(int *)0x44a2 != 0)) {
        local_4 = &param_5;
        *(undefined2 *)0x6028 = 4;
        local_6 = 2;
        goto LAB_2309_0264;
      }
      if ((in_BX == 1) || (in_BX == 2)) {
        local_4 = (undefined2 *)0x6984;
        *(undefined2 *)0x6028 = 0x382e;
        local_6 = 0;
        bVar3 = *(int *)0x449c == 0 && *(int *)0x449a == 0;
      }
      else {
        local_4 = &param_5;
        *(undefined2 *)0x6028 = 0x822;
        local_6 = 1;
        bVar3 = *(int *)0x44a0 == 0 && *(int *)0x449e == 0;
      }
    }
    else {
      local_4 = (undefined2 *)0x6984;
      *(undefined2 *)0x6028 = 0x311e;
      bVar3 = *(int *)0x4494 == 0 && *(int *)0x4492 == 0;
    }
    if (bVar3) {
      FUN_1ed0_03d6(*(int *)0x448c,*(int *)0x448c >> 0xf,1,0);
    }
  }
  else {
    *(undefined2 *)0x6028 = 0x1000;
    *(undefined2 *)0x5290 = param_5;
    *(undefined2 *)0x5292 = param_6;
    *(undefined2 *)0x4a84 = param_5;
    *(undefined2 *)0x4a86 = param_6;
    local_8 = (int *)0x5290;
    local_4 = (undefined2 *)0x6984;
  }
LAB_2309_0264:
  *(undefined2 *)0x4ea6 = 0;
  *(undefined2 *)0x4ea4 = 0;
  if (*(int *)0x4514 == 0 && *(int *)0x4512 == 0) {
    uVar4 = FUN_1cc9_0136(0x451a,unaff_DS);
    iVar2 = (int)((ulong)uVar4 >> 0x10);
    *(undefined2 *)0x4ea4 = (int)uVar4;
    *(int *)0x4ea6 = iVar2;
    if (iVar2 == 0 && *(int *)0x4ea4 == 0) {
      local_4[1] = 0;
      *local_4 = 0;
      goto LAB_2309_0327;
    }
  }
  else {
    uVar1 = *(undefined2 *)0x4514;
    *(undefined2 *)0x4ea4 = *(undefined2 *)0x4512;
    *(undefined2 *)0x4ea6 = uVar1;
  }
  if ((in_AX == 1) && (in_BX == 0)) {
    in_BX = FUN_2202_0086();
    if (in_BX == 0) goto LAB_2309_0327;
    local_4[1] = 0;
    *local_4 = 0;
    in_AX = local_6 + 1000;
  }
  else {
    do {
      if ((local_8[1] < 0) || ((local_8[1] < 1 && (*local_8 == 0)))) goto LAB_2309_0327;
      iVar2 = FUN_2202_0086();
    } while (iVar2 == 0);
    local_4[1] = 0;
    *local_4 = 0;
  }
  FUN_1ed0_03d6(in_BX,in_BX >> 0xf,in_AX,in_AX >> 0xf);
LAB_2309_0327:
  if (*(int *)0x4514 == 0 && *(int *)0x4512 == 0) {
    if (*(int *)0x4ea6 != 0 || *(int *)0x4ea4 != 0) {
      FUN_1cc9_0310(*(undefined2 *)0x4ea4,*(undefined2 *)0x4ea6);
    }
  }
  else {
    FUN_2309_0014();
  }
  return CONCAT22(local_4[1],*local_4);
}



int __cdecl16far FUN_233e_000a(byte *param_1)

{
  undefined2 uVar1;
  
  uVar1 = (undefined2)((ulong)param_1 >> 0x10);
  return (uint)*param_1 * 0x26 + (uint)((byte *)param_1)[1] * 0x4c +
         (uint)((byte *)param_1)[2] * 0xe;
}



undefined2 __cdecl16far FUN_2343_0004(void)

{
  int iVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x3c68 != 0) {
    iVar1 = (*(code *)*(undefined2 *)0x3c6c)(0x2343);
    if (iVar1 != 0) {
      return 0;
    }
  }
  return 0xffff;
}



int __cdecl16far FUN_2345_000a(int param_1,uint param_2)

{
  uint uVar1;
  int iVar2;
  uint in_DX;
  uint uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  undefined2 uVar7;
  undefined2 unaff_DS;
  bool bVar8;
  long lVar9;
  uint local_12;
  int local_8;
  
  local_8 = -1;
  lVar9 = 0;
  if (*(int *)0x4504 != 0) {
    uVar1 = FUN_2388_0d82(param_1,param_2,0x5c);
    uVar3 = in_DX | uVar1;
    local_12 = in_DX;
    if (uVar3 == 0) {
      uVar1 = FUN_2388_0ca8(param_1,param_2,0x2a);
      local_12 = uVar3;
    }
    if (local_12 == 0 && uVar1 == 0) {
      local_12 = param_2;
      iVar5 = param_1;
    }
    else {
      iVar5 = uVar1 + 1;
    }
    if (*(char *)0x44fa == '\x03') {
      iVar2 = FUN_2290_0006();
    }
    else {
      lVar9 = FUN_1cc9_0136(0x4522,unaff_DS);
      *(undefined2 *)0x49c8 = (int)lVar9;
      *(undefined2 *)0x49ca = (int)((ulong)lVar9 >> 0x10);
      if (lVar9 == 0) goto LAB_2345_0106;
      iVar2 = FUN_2343_0004(0x34bc,0,*(undefined2 *)0x4500,0,0,0,lVar9);
    }
    if (iVar2 == 0) {
      iVar2 = 0;
      iVar6 = 0;
      do {
        if (-1 < local_8) break;
        uVar7 = (undefined2)((ulong)*(undefined4 *)0x49c8 >> 0x10);
        iVar4 = (int)*(undefined4 *)0x49c8;
        if (*(char *)(iVar4 + iVar6) == -1) {
          bVar8 = param_2 == 0 && param_1 == 0;
        }
        else {
          iVar4 = FUN_2388_0cd6(iVar5,local_12,iVar4 + iVar6 + 3,uVar7);
          bVar8 = iVar4 == 0;
        }
        if (bVar8) {
          local_8 = iVar2;
        }
        iVar2 = iVar2 + 1;
        iVar6 = iVar6 + 0x5a;
      } while (iVar6 < 0x34bc);
    }
  }
LAB_2345_0106:
  if (-1 < local_8) {
    FUN_2292_000a();
  }
  if (lVar9 != 0) {
    FUN_1cc9_0310(lVar9);
  }
  return local_8;
}



undefined4 __stdcall16far FUN_2357_000e(undefined4 param_1)

{
  int iVar1;
  int iVar2;
  
  iVar2 = (int)((ulong)param_1 >> 0x10);
  iVar1 = (int)param_1;
  if (iVar1 < 0) {
    iVar1 = iVar1 + -0x8000;
    iVar2 = iVar2 + 0x800;
  }
  return CONCAT22(iVar2,iVar1);
}



uint __stdcall16far FUN_2359_0006(uint *param_1,undefined1 *param_2)

{
  uint *puVar1;
  undefined1 *puVar2;
  undefined1 *puVar3;
  int iVar4;
  undefined1 *puVar5;
  uint uVar6;
  uint uVar7;
  undefined1 *puVar8;
  undefined1 *puVar9;
  undefined2 unaff_DS;
  undefined4 uVar10;
  
  uVar7 = *param_1;
  uVar6 = *(uint *)0x5290;
  iVar4 = *(int *)0x5292;
  if (iVar4 != -1) {
    if (uVar6 == 0 && iVar4 == 0) goto LAB_2359_005a;
    if ((iVar4 == 0) && (uVar6 < uVar7)) {
      uVar7 = uVar6;
    }
    *(int *)0x5290 = uVar6 - uVar7;
    *(int *)0x5292 = iVar4 - (uint)(uVar6 < uVar7);
  }
  uVar6 = uVar7;
  puVar1 = (uint *)0x4b0a;
  uVar7 = *puVar1;
  *puVar1 = *puVar1 + uVar6;
  *(int *)0x4b0c = *(int *)0x4b0c + (uint)CARRY2(uVar7,uVar6);
  if (uVar6 != 0) {
    puVar9 = (undefined1 *)param_2;
    puVar5 = (undefined1 *)*(undefined4 *)0x6348;
    puVar8 = (undefined1 *)puVar5;
    for (uVar7 = uVar6; uVar7 != 0; uVar7 = uVar7 - 1) {
      puVar3 = puVar9;
      puVar9 = puVar9 + 1;
      puVar2 = puVar8;
      puVar8 = puVar8 + 1;
      *puVar3 = *puVar2;
    }
    *(undefined2 *)0x6348 = puVar8;
  }
LAB_2359_005a:
  uVar10 = FUN_204f_0002(*(undefined2 *)0x6348,*(undefined2 *)0x634a);
  *(undefined2 *)0x6348 = (int)uVar10;
  *(undefined2 *)0x634a = (int)((ulong)uVar10 >> 0x10);
  return uVar6;
}



void __stdcall16far FUN_2360_000a(uint *param_1,undefined1 *param_2)

{
  uint *puVar1;
  uint uVar2;
  undefined1 *puVar3;
  undefined1 *puVar4;
  int iVar5;
  undefined1 *puVar6;
  uint uVar7;
  undefined1 *puVar8;
  undefined1 *puVar9;
  undefined2 unaff_DS;
  undefined4 uVar10;
  
  uVar7 = *param_1;
  uVar2 = *(uint *)0x4a84;
  iVar5 = *(int *)0x4a86;
  if (iVar5 != -1) {
    if (uVar2 == 0 && iVar5 == 0) goto LAB_2360_005d;
    if ((iVar5 == 0) && (uVar2 < uVar7)) {
      uVar7 = uVar2;
    }
    *(int *)0x4a84 = uVar2 - uVar7;
    *(int *)0x4a86 = iVar5 - (uint)(uVar2 < uVar7);
  }
  puVar1 = (uint *)0x6984;
  uVar2 = *puVar1;
  *puVar1 = *puVar1 + uVar7;
  *(int *)0x6986 = *(int *)0x6986 + (uint)CARRY2(uVar2,uVar7);
  if (uVar7 != 0) {
    puVar6 = (undefined1 *)*(undefined4 *)0x5a9e;
    puVar9 = (undefined1 *)puVar6;
    puVar8 = (undefined1 *)param_2;
    for (; uVar7 != 0; uVar7 = uVar7 - 1) {
      puVar4 = puVar9;
      puVar9 = puVar9 + 1;
      puVar3 = puVar8;
      puVar8 = puVar8 + 1;
      *puVar4 = *puVar3;
    }
    *(undefined2 *)0x5a9e = puVar9;
  }
LAB_2360_005d:
  uVar10 = FUN_204f_0002(*(undefined2 *)0x5a9e,*(undefined2 *)0x5aa0);
  *(undefined2 *)0x5a9e = (int)uVar10;
  *(undefined2 *)0x5aa0 = (int)((ulong)uVar10 >> 0x10);
  return;
}



uint __stdcall16far FUN_2367_0008(uint *param_1,undefined2 param_2,undefined2 param_3)

{
  uint *puVar1;
  uint uVar2;
  uint uVar3;
  undefined2 unaff_DS;
  
  if (*(int *)0x5292 < 0) {
    uVar3 = *param_1;
  }
  else {
    uVar3 = *param_1;
    if (((int)*(uint *)0x5292 < 1) && ((0x7fff < *(uint *)0x5292 || (*(uint *)0x5290 < uVar3)))) {
      uVar3 = *(uint *)0x5290;
    }
  }
  uVar2 = 0;
  if (uVar3 != 0) {
    uVar2 = FUN_1bca_0000(uVar3,0,param_2,param_3);
    if ((-1 < *(int *)0x5292) && ((0 < *(int *)0x5292 || (*(int *)0x5290 != 0)))) {
      puVar1 = (uint *)0x5290;
      uVar3 = *puVar1;
      *puVar1 = *puVar1 - uVar2;
      *(int *)0x5292 = *(int *)0x5292 - (uint)(uVar3 < uVar2);
    }
    puVar1 = (uint *)0x4b0a;
    uVar3 = *puVar1;
    *puVar1 = *puVar1 + uVar2;
    *(int *)0x4b0c = *(int *)0x4b0c + (uint)CARRY2(uVar3,uVar2);
  }
  return uVar2;
}



uint __stdcall16far FUN_2378_0004(uint *param_1,undefined2 param_2,undefined2 param_3)

{
  uint *puVar1;
  uint uVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  undefined2 unaff_DS;
  undefined4 uVar6;
  int iVar7;
  undefined2 uVar8;
  uint uVar9;
  
  uVar6 = CONCAT22(param_3,param_2);
  if (*(int *)0x4a86 < 0) {
    uVar3 = *param_1;
  }
  else {
    uVar3 = *param_1;
    if (((int)*(uint *)0x4a86 < 1) && ((0x7fff < *(uint *)0x4a86 || (*(uint *)0x4a84 < uVar3)))) {
      uVar3 = *(uint *)0x4a84;
    }
  }
  if (uVar3 != 0) {
    iVar7 = *(int *)0x445c;
    uVar8 = *(undefined2 *)0x445e;
    uVar9 = uVar3;
    do {
      if (0x3fff < *(int *)0x4e82) {
        iVar4 = FUN_2145_01c4();
        *(int *)0x4bb4 = iVar4;
        *(undefined2 *)0x4e82 = 0;
        if (iVar4 < 0) {
          return uVar3;
        }
      }
      uVar5 = 0x4000U - *(int *)0x4e82;
      if ((int)uVar9 < (int)(0x4000U - *(int *)0x4e82)) {
        uVar5 = uVar9;
      }
      FUN_212d_00d4(0,*(undefined2 *)0x4bb4,uVar5,iVar7,uVar8,uVar9,uVar6);
      FUN_2388_0c4a(iVar7 + *(int *)0x4e82,uVar8,uVar6,uVar5);
      uVar6 = FUN_204f_0002((int)uVar6 + uVar5,(int)((ulong)uVar6 >> 0x10));
      uVar9 = uVar9 - uVar5;
      *(int *)0x4e82 = *(int *)0x4e82 + uVar5;
      if ((-1 < *(int *)0x4a86) && ((0 < *(int *)0x4a86 || (*(int *)0x4a84 != 0)))) {
        puVar1 = (uint *)0x4a84;
        uVar2 = *puVar1;
        *puVar1 = *puVar1 - uVar5;
        *(int *)0x4a86 = *(int *)0x4a86 - (uint)(uVar2 < uVar5);
      }
      puVar1 = (uint *)0x6984;
      uVar2 = *puVar1;
      *puVar1 = *puVar1 + uVar5;
      *(int *)0x6986 = *(int *)0x6986 + (uint)CARRY2(uVar2,uVar5);
    } while (uVar9 != 0);
  }
  return uVar3;
}



// WARNING: Stack frame is not setup normally: Input value of stackpointer is not used
// WARNING: This function may have set the stack pointer

void __cdecl16far entry(void)

{
  undefined1 *puVar1;
  code *pcVar2;
  code *pcVar3;
  byte bVar4;
  int iVar5;
  undefined1 *puVar6;
  undefined1 *puVar7;
  undefined1 *puVar8;
  undefined1 *puVar9;
  undefined2 *puVar10;
  undefined1 *puVar11;
  uint uVar12;
  undefined1 *puVar13;
  undefined2 unaff_ES;
  undefined2 uVar14;
  undefined2 unaff_DS;
  undefined2 uVar15;
  
  uVar15 = 0x2388;
  puVar6 = (undefined1 *)0x1000;
  pcVar3 = (code *)swi(0x21);
  bVar4 = (*pcVar3)();
  if (bVar4 < 2) {
    *(undefined2 *)(puVar6 + -2) = unaff_ES;
    *(undefined2 *)(puVar6 + -4) = 0;
    return;
  }
  uVar12 = *(int *)0x2 + 0xda19;
  if (0xfff < uVar12) {
    uVar12 = 0x1000;
  }
  puVar7 = puVar6 + 0x69ae;
  puVar13 = puVar6 + 0x69ae;
  if ((undefined1 *)0x9651 < puVar6) {
    *(undefined2 *)(puVar6 + 0x69ac) = 0x25e7;
    unaff_DS = *(undefined2 *)(puVar6 + 0x69ac);
    *(undefined2 *)(puVar6 + 0x69ac) = 0x2388;
    *(undefined2 *)(puVar6 + 0x69aa) = 0x38cd;
    FUN_2388_0f7a();
    *(undefined2 *)(puVar6 + 0x69ac) = 0;
    *(undefined2 *)(puVar6 + 0x69aa) = 0x2388;
    *(undefined2 *)(puVar6 + 0x69a8) = 0x38d4;
    FUN_2388_1201();
    pcVar3 = (code *)swi(0x21);
    (*pcVar3)();
    puVar13 = puVar7;
  }
  DAT_25e7_4532 = uVar12 * 0x10 + -1;
  DAT_25e7_4534 = 0x25e7;
  puVar8 = (undefined1 *)((uint)puVar13 & 0xfffe);
  DAT_25e7_453e = puVar8 + -2;
  DAT_25e7_4538 = puVar8;
  *(undefined2 *)(puVar8 + -2) = 0xfffe;
  puVar9 = puVar8 + -4;
  DAT_25e7_453a = puVar8 + -4;
  DAT_25e7_453c = puVar8 + -4;
  DAT_25e7_452e = puVar8 + -4;
  *(undefined2 *)(puVar8 + -4) = 1;
  *(int *)0x2 = uVar12 + 0x25e7;
  pcVar3 = (code *)swi(0x21);
  (*pcVar3)();
  DAT_25e7_456e = unaff_DS;
  *(undefined2 *)(puVar9 + -2) = 0x25e7;
  uVar14 = *(undefined2 *)(puVar9 + -2);
  puVar13 = (undefined1 *)0x499a;
  for (iVar5 = 0x2016; iVar5 != 0; iVar5 = iVar5 + -1) {
    puVar1 = puVar13;
    puVar13 = puVar13 + 1;
    *puVar1 = 0;
  }
  *(undefined2 *)(puVar9 + -2) = 0x25e7;
  pcVar2 = (code *)*(int *)0x4894;
  if (pcVar2 != (code *)0x0) {
    uVar15 = 0x2000;
    puVar10 = (undefined2 *)(puVar9 + -2);
    puVar9 = puVar9 + -2;
    *puVar10 = 0x3937;
    (*pcVar2)();
  }
  *(undefined2 *)(puVar9 + -2) = uVar15;
  *(undefined2 *)(puVar9 + -4) = 0x393c;
  FUN_2388_1158();
  *(undefined2 *)(puVar9 + -2) = 0x2388;
  puVar11 = puVar9 + -4;
  *(undefined2 *)(puVar9 + -4) = 0x3941;
  FUN_2388_0fc6();
  *(undefined2 *)(puVar11 + -2) = 0x2388;
  *(undefined2 *)(puVar11 + -4) = 0x3948;
  FUN_2388_0116();
  *(undefined2 *)(puVar11 + -2) = 0x25e7;
  uVar15 = *(undefined2 *)(puVar11 + -2);
  *(undefined2 *)(puVar11 + -2) = *(undefined2 *)0x458f;
  *(undefined2 *)(puVar11 + -4) = *(undefined2 *)0x458d;
  *(undefined2 *)(puVar11 + -6) = *(undefined2 *)0x458b;
  *(undefined2 *)(puVar11 + -8) = 0x2388;
  *(undefined2 *)(puVar11 + -10) = 0x395b;
  uVar15 = FUN_1000_28d8();
  *(undefined2 *)(puVar11 + -8) = uVar15;
  *(undefined2 *)(puVar11 + -10) = 0x1000;
  *(undefined2 *)(puVar11 + -0xc) = 0x3960;
  FUN_2388_01db();
  return;
}



void FUN_2388_00f1(void)

{
  char *pcVar1;
  byte *pbVar2;
  byte *pbVar3;
  byte *pbVar4;
  code *pcVar5;
  undefined2 in_AX;
  undefined2 uVar6;
  int iVar7;
  uint extraout_DX;
  int in_BX;
  int iVar8;
  int unaff_SI;
  byte *pbVar9;
  byte *pbVar10;
  undefined2 unaff_ES;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar11;
  
  uVar6 = 0x2388;
  FUN_2388_0f7a();
  FUN_2388_1201(in_AX);
  if (*(int *)0x4896 == -0x292a) {
    uVar6 = 0x2000;
    (*(code *)*(undefined2 *)0x489a)();
  }
  uVar6 = (*(code *)*(undefined2 *)0x4530)(uVar6,0xff);
  out(0x25,uVar6);
  pcVar1 = (char *)(in_BX + unaff_SI + 0x3500);
  *pcVar1 = *pcVar1 + (char)((uint)in_BX >> 8);
  pcVar5 = (code *)swi(0x21);
  (*pcVar5)();
  *(int *)0x455a = in_BX;
  *(undefined2 *)0x455c = unaff_ES;
  pcVar5 = (code *)swi(0x21);
  (*pcVar5)();
  if (*(int *)0x48a8 != 0) {
    bVar11 = false;
    (*(code *)*(undefined2 *)0x48a6)();
    if (bVar11) {
      FUN_2388_0f9c();
      return;
    }
    (*(code *)*(undefined2 *)0x48a6)();
  }
  iVar8 = *(int *)0x2c;
  if (iVar8 != 0) {
    pbVar10 = (byte *)0x0;
    do {
      if (*pbVar10 == 0) break;
      iVar7 = 0xd;
      pbVar9 = (byte *)0x454c;
      bVar11 = false;
      do {
        if (iVar7 == 0) break;
        iVar7 = iVar7 + -1;
        pbVar4 = pbVar10;
        pbVar10 = pbVar10 + 1;
        pbVar2 = pbVar9;
        pbVar9 = pbVar9 + 1;
        bVar11 = *pbVar2 == *pbVar4;
      } while (bVar11);
      if (bVar11) {
        pbVar9 = (byte *)0x4577;
        goto LAB_2388_0198;
      }
      iVar7 = 0x7fff;
      bVar11 = true;
      do {
        if (iVar7 == 0) break;
        iVar7 = iVar7 + -1;
        pbVar2 = pbVar10;
        pbVar10 = pbVar10 + 1;
        bVar11 = *pbVar2 == 0;
      } while (!bVar11);
    } while (bVar11);
  }
LAB_2388_01ac:
  iVar8 = 4;
  do {
    bVar11 = false;
    *(byte *)(iVar8 + 0x4577) = *(byte *)(iVar8 + 0x4577) & 0xbf;
    pcVar5 = (code *)swi(0x21);
    (*pcVar5)();
    if ((!bVar11) && ((extraout_DX & 0x80) != 0)) {
      *(byte *)(iVar8 + 0x4577) = *(byte *)(iVar8 + 0x4577) | 0x40;
    }
    iVar8 = iVar8 + -1;
  } while (-1 < iVar8);
  FUN_2388_028b();
  FUN_2388_028b();
  return;
LAB_2388_0198:
  pbVar2 = pbVar10;
  pbVar3 = pbVar10 + 1;
  if (*pbVar2 < 0x41) goto LAB_2388_01ac;
  pbVar10 = pbVar10 + 2;
  if (*pbVar3 < 0x41) goto LAB_2388_01ac;
  pbVar4 = pbVar9;
  pbVar9 = pbVar9 + 1;
  *pbVar4 = *pbVar3 + 0xbf | (*pbVar2 + 0xbf) * '\x10';
  goto LAB_2388_0198;
}



void __cdecl16far FUN_2388_0116(void)

{
  byte *pbVar1;
  byte *pbVar2;
  byte *pbVar3;
  code *pcVar4;
  int iVar5;
  uint extraout_DX;
  undefined2 in_BX;
  int iVar6;
  byte *pbVar7;
  byte *pbVar8;
  undefined2 unaff_ES;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar9;
  
  pcVar4 = (code *)swi(0x21);
  (*pcVar4)();
  *(undefined2 *)0x455a = in_BX;
  *(undefined2 *)0x455c = unaff_ES;
  pcVar4 = (code *)swi(0x21);
  (*pcVar4)();
  if (*(int *)0x48a8 != 0) {
    bVar9 = false;
    (*(code *)*(undefined2 *)0x48a6)();
    if (bVar9) {
      FUN_2388_0f9c();
      return;
    }
    (*(code *)*(undefined2 *)0x48a6)();
  }
  iVar6 = *(int *)0x2c;
  if (iVar6 != 0) {
    pbVar8 = (byte *)0x0;
    do {
      if (*pbVar8 == 0) break;
      iVar5 = 0xd;
      pbVar7 = (byte *)0x454c;
      bVar9 = false;
      do {
        if (iVar5 == 0) break;
        iVar5 = iVar5 + -1;
        pbVar3 = pbVar8;
        pbVar8 = pbVar8 + 1;
        pbVar1 = pbVar7;
        pbVar7 = pbVar7 + 1;
        bVar9 = *pbVar1 == *pbVar3;
      } while (bVar9);
      if (bVar9) {
        pbVar7 = (byte *)0x4577;
        goto LAB_2388_0198;
      }
      iVar5 = 0x7fff;
      bVar9 = true;
      do {
        if (iVar5 == 0) break;
        iVar5 = iVar5 + -1;
        pbVar1 = pbVar8;
        pbVar8 = pbVar8 + 1;
        bVar9 = *pbVar1 == 0;
      } while (!bVar9);
    } while (bVar9);
  }
LAB_2388_01ac:
  iVar6 = 4;
  do {
    bVar9 = false;
    *(byte *)(iVar6 + 0x4577) = *(byte *)(iVar6 + 0x4577) & 0xbf;
    pcVar4 = (code *)swi(0x21);
    (*pcVar4)();
    if ((!bVar9) && ((extraout_DX & 0x80) != 0)) {
      *(byte *)(iVar6 + 0x4577) = *(byte *)(iVar6 + 0x4577) | 0x40;
    }
    iVar6 = iVar6 + -1;
  } while (-1 < iVar6);
  FUN_2388_028b();
  FUN_2388_028b();
  return;
LAB_2388_0198:
  pbVar1 = pbVar8;
  pbVar2 = pbVar8 + 1;
  if (*pbVar1 < 0x41) goto LAB_2388_01ac;
  pbVar8 = pbVar8 + 2;
  if (*pbVar2 < 0x41) goto LAB_2388_01ac;
  pbVar3 = pbVar7;
  pbVar7 = pbVar7 + 1;
  *pbVar3 = *pbVar2 + 0xbf | (*pbVar1 + 0xbf) * '\x10';
  goto LAB_2388_0198;
}



void __cdecl16far FUN_2388_01db(void)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  FUN_2388_028b();
  FUN_2388_028b();
  if (*(int *)0x4896 == -0x292a) {
    (*(code *)*(undefined2 *)0x489c)();
  }
  FUN_2388_028b();
  FUN_2388_028b();
  FUN_2388_0fa2();
  FUN_2388_025e();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  return;
}



void __cdecl16near FUN_2388_025e(undefined2 param_1)

{
  code *pcVar1;
  undefined2 unaff_DS;
  
  if (*(int *)0x48a8 != 0) {
    (*(code *)*(undefined2 *)0x48a6)(0x2388);
  }
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  if (*(char *)0x4598 != '\0') {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
  }
  return;
}



void __cdecl16near FUN_2388_028b(void)

{
  int *piVar1;
  int *unaff_SI;
  int *unaff_DI;
  int *piVar2;
  undefined2 unaff_DS;
  
  while (unaff_SI < unaff_DI) {
    piVar2 = unaff_DI + -2;
    piVar1 = unaff_DI + -1;
    unaff_DI = piVar2;
    if (*piVar2 != 0 || *piVar1 != 0) {
      (*(code *)*piVar2)(0x2388);
    }
  }
  return;
}



// WARNING (jumptable): Unable to track spacebase fully for stack
// WARNING: Unable to track spacebase fully for stack

void __cdecl16far FUN_2388_029e(void)

{
  int iVar1;
  undefined1 *in_AX;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined2 in_stack_00000000;
  undefined2 in_stack_00000002;
  
  iVar1 = -(int)in_AX;
  if ((in_AX <= &stack0x0004) && ((undefined1 *)*(uint *)0x45a2 <= &stack0x0004 + iVar1)) {
    *(undefined2 *)(&stack0x0002 + iVar1) = in_stack_00000002;
    *(undefined2 *)(&stack0x0000 + iVar1) = in_stack_00000000;
    return;
  }
  if (*(int *)0x459e == -1) {
    FUN_2388_00f1();
    return;
  }
                    // WARNING: Could not recover jumptable at 0x00023b3e. Too many branches
                    // WARNING: Treating indirect jump as call
  (*(code *)(ulong)*(uint *)0x459e)();
  return;
}



undefined2 __cdecl16far FUN_2388_02c2(int param_1)

{
  int iVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  char local_10;
  undefined1 uStack_f;
  undefined1 local_e [8];
  int local_6;
  undefined1 *local_4;
  
  uVar2 = 0xffff;
  if (((*(byte *)(param_1 + 6) & 0x40) != 0) || ((*(byte *)(param_1 + 6) & 0x83) == 0))
  goto LAB_2388_0370;
  uVar2 = FUN_2388_15ce(param_1);
  local_6 = *(int *)(param_1 + 0xa4);
  FUN_2388_1408(param_1);
  iVar1 = FUN_2388_1bb2(0x2388,*(undefined1 *)(param_1 + 7));
  if (-1 < iVar1) {
    if (local_6 == 0) goto LAB_2388_0370;
    FUN_2388_0626(&local_10,0x45a4);
    local_4 = local_e;
    if (local_10 == '\\') {
      local_4 = &uStack_f;
    }
    else {
      FUN_2388_05e6(&local_10,0x45a6);
    }
    FUN_2388_073c(local_6,local_4,10);
    iVar1 = FUN_2388_0aa6(0x2388,&local_10);
    if (iVar1 == 0) goto LAB_2388_0370;
  }
  uVar2 = 0xffff;
LAB_2388_0370:
  *(undefined1 *)(param_1 + 6) = 0;
  return uVar2;
}



undefined2 __cdecl16far FUN_2388_037c(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  int iVar1;
  undefined2 uVar2;
  
  iVar1 = FUN_2388_1b7e();
  if (iVar1 == 0) {
    uVar2 = 0;
  }
  else {
    uVar2 = FUN_2388_1434(param_1,param_2,param_3,iVar1);
  }
  return uVar2;
}



void __cdecl16far FUN_2388_03a8(undefined2 param_1,undefined2 param_2)

{
  FUN_2388_037c(param_1,param_2,0);
  return;
}



uint __cdecl16far FUN_2388_03be(undefined1 *param_1,uint param_2,uint param_3,int *param_4)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  undefined2 unaff_DS;
  uint uVar5;
  uint local_6;
  
  uVar1 = param_2 * param_3;
  if (uVar1 == 0) {
    param_3 = 0;
  }
  else {
    uVar5 = uVar1;
    if (((*(byte *)(param_4 + 3) & 0xc) == 0) && ((*(byte *)(param_4 + 0x50) & 1) == 0)) {
      local_6 = 0x200;
    }
    else {
      local_6 = param_4[0x51];
    }
    do {
      if ((((*(byte *)(param_4 + 3) & 0xc) == 0) && ((*(byte *)(param_4 + 0x50) & 1) == 0)) ||
         (uVar2 = param_4[1], uVar2 == 0)) {
        if (uVar5 < local_6) {
          uVar4 = uVar5;
          iVar3 = FUN_2388_128e(param_4);
          if (iVar3 == -1) break;
          *param_1 = (char)iVar3;
          param_1 = param_1 + 1;
          uVar4 = uVar4 - 1;
          local_6 = param_4[0x51];
        }
        else {
          uVar4 = uVar5 - uVar5 % local_6;
          iVar3 = FUN_2388_1c4c(0x2388,*(undefined1 *)((int)param_4 + 7),param_1,uVar4,uVar5,param_1
                               );
          if (iVar3 == 0) {
            *(byte *)(param_4 + 3) = *(byte *)(param_4 + 3) | 0x10;
            break;
          }
          if (iVar3 == -1) {
            *(byte *)(param_4 + 3) = *(byte *)(param_4 + 3) | 0x20;
            break;
          }
          uVar4 = uVar4 - iVar3;
          param_1 = (undefined1 *)(uVar5 + iVar3);
        }
      }
      else {
        if (uVar5 < uVar2) {
          uVar2 = uVar5;
        }
        FUN_2388_212c(param_1,*param_4,uVar2);
        uVar4 = uVar5 - uVar2;
        param_4[1] = param_4[1] - uVar2;
        param_1 = param_1 + uVar2;
        *param_4 = *param_4 + uVar2;
      }
      uVar5 = uVar4;
    } while (uVar4 != 0);
    if (uVar4 != 0) {
      param_3 = (uVar1 - uVar4) / param_2;
    }
  }
  return param_3;
}



uint __cdecl16far FUN_2388_04a2(undefined1 *param_1,uint param_2,uint param_3,int *param_4)

{
  uint uVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  byte *pbVar6;
  undefined2 unaff_DS;
  uint local_6;
  
  uVar1 = param_2 * param_3;
  if (uVar1 == 0) {
    param_3 = 0;
  }
  else {
    pbVar6 = (byte *)(param_4 + 0x50);
    uVar5 = uVar1;
    if (((*(byte *)(param_4 + 3) & 0xc) == 0) && ((*pbVar6 & 1) == 0)) {
      local_6 = 0x200;
    }
    else {
      local_6 = param_4[0x51];
    }
    do {
      if ((((*(byte *)(param_4 + 3) & 8) == 0) && ((*pbVar6 & 1) == 0)) ||
         (uVar2 = param_4[1], uVar2 == 0)) {
        if (uVar5 < local_6) {
          iVar3 = FUN_2388_1324(*param_1,param_4);
          if (iVar3 == -1) break;
          param_1 = param_1 + 1;
          uVar5 = uVar5 - 1;
          local_6 = param_4[0x51];
          if (local_6 == 0) {
            local_6 = 1;
          }
        }
        else {
          if ((((*(byte *)(param_4 + 3) & 8) != 0) || ((*pbVar6 & 1) != 0)) &&
             (iVar3 = FUN_2388_15ce(param_4), iVar3 != 0)) break;
          iVar3 = uVar5 - uVar5 % local_6;
          iVar4 = FUN_2388_1d36(0x2388,*(undefined1 *)((int)param_4 + 7),param_1,iVar3);
          if ((iVar4 == -1) || (uVar5 = uVar5 - iVar4, iVar4 != iVar3)) {
            *(byte *)(param_4 + 3) = *(byte *)(param_4 + 3) | 0x20;
            break;
          }
          param_1 = param_1 + iVar4;
        }
      }
      else {
        if (uVar5 < uVar2) {
          uVar2 = uVar5;
        }
        FUN_2388_212c(*param_4,param_1,uVar2);
        uVar5 = uVar5 - uVar2;
        param_4[1] = param_4[1] - uVar2;
        param_1 = param_1 + uVar2;
        *param_4 = *param_4 + uVar2;
      }
    } while (uVar5 != 0);
    if (uVar5 != 0) {
      param_3 = (uVar1 - uVar5) / param_2;
    }
  }
  return param_3;
}



undefined2 __cdecl16far FUN_2388_05a8(undefined2 param_1)

{
  undefined2 uVar1;
  undefined2 uVar2;
  
  uVar1 = FUN_2388_151c(0x46ce);
  uVar2 = FUN_2388_16a6(0x46ce,param_1,&stack0x0006);
  FUN_2388_158f(uVar1,0x46ce);
  return uVar2;
}



char * __cdecl16far FUN_2388_05e6(char *param_1,char *param_2)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  char *pcVar6;
  char *pcVar7;
  undefined2 unaff_DS;
  
  iVar3 = -1;
  pcVar6 = param_1;
  do {
    if (iVar3 == 0) break;
    iVar3 = iVar3 + -1;
    pcVar1 = pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (*pcVar1 != '\0');
  uVar4 = 0xffff;
  do {
    if (uVar4 == 0) break;
    uVar4 = uVar4 - 1;
    pcVar1 = param_2;
    param_2 = param_2 + 1;
  } while (*pcVar1 != '\0');
  uVar4 = ~uVar4;
  param_2 = param_2 + -uVar4;
  pcVar7 = pcVar6 + -1;
  if (((uint)param_2 & 1) != 0) {
    pcVar1 = param_2;
    param_2 = param_2 + 1;
    pcVar6[-1] = *pcVar1;
    uVar4 = uVar4 - 1;
    pcVar7 = pcVar6;
  }
  for (uVar5 = uVar4 >> 1; uVar5 != 0; uVar5 = uVar5 - 1) {
    pcVar2 = pcVar7;
    pcVar7 = pcVar7 + 2;
    pcVar1 = param_2;
    param_2 = param_2 + 2;
    *(undefined2 *)pcVar2 = *(undefined2 *)pcVar1;
  }
  for (uVar4 = (uint)((uVar4 & 1) != 0); uVar4 != 0; uVar4 = uVar4 - 1) {
    pcVar2 = pcVar7;
    pcVar7 = pcVar7 + 1;
    pcVar1 = param_2;
    param_2 = param_2 + 1;
    *pcVar2 = *pcVar1;
  }
  return param_1;
}



void __cdecl16far FUN_2388_0626(char *param_1,char *param_2)

{
  char *pcVar1;
  char *pcVar2;
  uint uVar3;
  uint uVar4;
  char *pcVar5;
  undefined2 unaff_DS;
  
  uVar3 = 0xffff;
  pcVar5 = param_2;
  do {
    if (uVar3 == 0) break;
    uVar3 = uVar3 - 1;
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 1;
  } while (*pcVar1 != '\0');
  uVar3 = ~uVar3;
  if (((uint)param_1 & 1) != 0) {
    pcVar2 = param_1;
    param_1 = param_1 + 1;
    pcVar1 = param_2;
    param_2 = param_2 + 1;
    *pcVar2 = *pcVar1;
    uVar3 = uVar3 - 1;
  }
  for (uVar4 = uVar3 >> 1; uVar4 != 0; uVar4 = uVar4 - 1) {
    pcVar2 = param_1;
    param_1 = param_1 + 2;
    pcVar1 = param_2;
    param_2 = param_2 + 2;
    *(undefined2 *)pcVar2 = *(undefined2 *)pcVar1;
  }
  for (uVar3 = (uint)((uVar3 & 1) != 0); uVar3 != 0; uVar3 = uVar3 - 1) {
    pcVar2 = param_1;
    param_1 = param_1 + 1;
    pcVar1 = param_2;
    param_2 = param_2 + 1;
    *pcVar2 = *pcVar1;
  }
  return;
}



int __cdecl16far FUN_2388_0658(byte *param_1,char *param_2)

{
  byte *pbVar1;
  char *pcVar2;
  byte *pbVar3;
  int iVar4;
  uint uVar5;
  char *pcVar6;
  byte *pbVar7;
  undefined2 unaff_DS;
  bool bVar8;
  bool bVar9;
  
  iVar4 = 0;
  uVar5 = 0xffff;
  do {
    if (uVar5 == 0) break;
    uVar5 = uVar5 - 1;
    pcVar2 = param_2;
    param_2 = param_2 + 1;
  } while (*pcVar2 != '\0');
  pcVar6 = (char *)~uVar5;
  bVar8 = param_2 < pcVar6;
  pbVar7 = (byte *)(param_2 + -(int)pcVar6);
  bVar9 = pbVar7 == (byte *)0x0;
  do {
    if (pcVar6 == (char *)0x0) break;
    pcVar6 = pcVar6 + -1;
    pbVar3 = pbVar7;
    pbVar7 = pbVar7 + 1;
    pbVar1 = param_1;
    param_1 = param_1 + 1;
    bVar8 = *pbVar1 < *pbVar3;
    bVar9 = *pbVar1 == *pbVar3;
  } while (bVar9);
  if (!bVar9) {
    iVar4 = (1 - (uint)bVar8) - (uint)(bVar8 != 0);
  }
  return iVar4;
}



int __cdecl16far FUN_2388_0684(char *param_1)

{
  char *pcVar1;
  uint uVar2;
  undefined2 unaff_DS;
  
  uVar2 = 0xffff;
  do {
    if (uVar2 == 0) break;
    uVar2 = uVar2 - 1;
    pcVar1 = param_1;
    param_1 = param_1 + 1;
  } while (*pcVar1 != '\0');
  return ~uVar2 - 1;
}



char * __cdecl16far FUN_2388_06a0(char *param_1,char *param_2,int param_3)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  char *pcVar4;
  char *pcVar5;
  undefined2 unaff_DS;
  bool bVar6;
  
  iVar3 = -1;
  pcVar4 = param_1;
  do {
    if (iVar3 == 0) break;
    iVar3 = iVar3 + -1;
    pcVar1 = pcVar4;
    pcVar4 = pcVar4 + 1;
  } while (*pcVar1 != '\0');
  pcVar4 = pcVar4 + -1;
  bVar6 = pcVar4 == (char *)0x0;
  iVar3 = param_3;
  pcVar5 = param_2;
  do {
    if (iVar3 == 0) break;
    iVar3 = iVar3 + -1;
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 1;
    bVar6 = *pcVar1 == '\0';
  } while (!bVar6);
  if (bVar6) {
    iVar3 = iVar3 + 1;
  }
  for (iVar3 = -(iVar3 - param_3); iVar3 != 0; iVar3 = iVar3 + -1) {
    pcVar2 = pcVar4;
    pcVar4 = pcVar4 + 1;
    pcVar1 = param_2;
    param_2 = param_2 + 1;
    *pcVar2 = *pcVar1;
  }
  *pcVar4 = '\0';
  return param_1;
}



char * __cdecl16far FUN_2388_06d6(char *param_1,char *param_2,int param_3)

{
  char *pcVar1;
  char *pcVar2;
  char *pcVar3;
  undefined2 unaff_DS;
  
  pcVar3 = param_1;
  if (param_3 != 0) {
    do {
      pcVar1 = param_2;
      param_2 = param_2 + 1;
      if (*pcVar1 == '\0') break;
      pcVar2 = pcVar3;
      pcVar3 = pcVar3 + 1;
      *pcVar2 = *pcVar1;
      param_3 = param_3 + -1;
    } while (param_3 != 0);
    for (; param_3 != 0; param_3 = param_3 + -1) {
      pcVar1 = pcVar3;
      pcVar3 = pcVar3 + 1;
      *pcVar1 = '\0';
    }
  }
  return param_1;
}



uint __cdecl16far FUN_2388_06fe(char *param_1,char *param_2,int param_3)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  uint uVar4;
  char *pcVar5;
  undefined2 unaff_DS;
  
  uVar4 = 0;
  iVar3 = param_3;
  pcVar5 = param_1;
  if (param_3 != 0) {
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      pcVar1 = pcVar5;
      pcVar5 = pcVar5 + 1;
    } while (*pcVar1 != '\0');
    param_3 = param_3 - iVar3;
    do {
      if (param_3 == 0) break;
      param_3 = param_3 + -1;
      pcVar2 = param_1;
      param_1 = param_1 + 1;
      pcVar1 = param_2;
      param_2 = param_2 + 1;
    } while (*pcVar1 == *pcVar2);
    uVar4 = 0;
    if ((byte)param_2[-1] <= (byte)param_1[-1]) {
      if (param_2[-1] == param_1[-1]) {
        return 0;
      }
      uVar4 = 0xfffe;
    }
    uVar4 = ~uVar4;
  }
  return uVar4;
}



int __cdecl16far thunk_FUN_2388_1e76(byte *param_1)

{
  byte *pbVar1;
  byte bVar2;
  byte bVar3;
  int iVar4;
  undefined2 unaff_DS;
  
  iVar4 = 0;
  do {
    do {
      pbVar1 = param_1;
      param_1 = param_1 + 1;
      bVar2 = *pbVar1;
    } while (bVar2 == 0x20);
  } while (bVar2 == 9);
  if ((bVar2 != 0x2d) && (bVar3 = bVar2, bVar2 != 0x2b)) goto LAB_2388_1e96;
  while( true ) {
    pbVar1 = param_1;
    param_1 = param_1 + 1;
    bVar3 = *pbVar1;
LAB_2388_1e96:
    if ((0x39 < bVar3) || (bVar3 < 0x30)) break;
    iVar4 = iVar4 * 10 + (uint)(byte)(bVar3 - 0x30);
  }
  if (bVar2 == 0x2d) {
    iVar4 = -iVar4;
  }
  return iVar4;
}



byte * __cdecl16far FUN_2388_073c(int param_1,byte *param_2,uint param_3)

{
  ulong uVar1;
  byte bVar2;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  byte *pbVar7;
  byte *pbVar8;
  byte *pbVar9;
  undefined2 unaff_DS;
  bool bVar10;
  char cVar3;
  
  uVar5 = 0;
  pbVar8 = param_2;
  pbVar7 = param_2;
  if ((param_3 == 10) && (uVar5 = param_1 >> 0xf, (int)uVar5 < 0)) {
    pbVar8 = param_2 + 1;
    *param_2 = 0x2d;
    bVar10 = param_1 != 0;
    param_1 = -param_1;
    uVar5 = -(uVar5 + bVar10);
    pbVar7 = pbVar8;
  }
  do {
    uVar6 = 0;
    uVar4 = uVar5;
    if (uVar5 != 0) {
      uVar4 = uVar5 / param_3;
      uVar6 = uVar5 % param_3;
    }
    uVar1 = CONCAT22(uVar6,param_1);
    param_1 = (int)(uVar1 / param_3);
    cVar3 = (char)(uVar1 % (ulong)param_3);
    bVar2 = cVar3 + 0x30;
    if (0x39 < bVar2) {
      bVar2 = cVar3 + 0x57;
    }
    pbVar9 = pbVar8 + 1;
    *pbVar8 = bVar2;
    uVar5 = uVar4;
    pbVar8 = pbVar9;
  } while (uVar4 != 0 || param_1 != 0);
  *pbVar9 = 0;
  do {
    pbVar9 = pbVar9 + -1;
    LOCK();
    bVar2 = *pbVar9;
    *pbVar9 = *pbVar7;
    UNLOCK();
    *pbVar7 = bVar2;
    pbVar8 = pbVar7 + 2;
    pbVar7 = pbVar7 + 1;
  } while (pbVar8 < pbVar9);
  return param_2;
}



void FUN_2388_0758(void)

{
  FUN_2388_2158();
  return;
}



undefined2 __cdecl16far FUN_2388_0762(undefined2 param_1,int *param_2)

{
  int iVar1;
  undefined2 uVar2;
  int in_DX;
  undefined2 unaff_DS;
  
  iVar1 = FUN_2388_1ef6(param_1);
  *param_2 = iVar1;
  param_2[1] = in_DX;
  if ((iVar1 == -1) && (in_DX == -1)) {
    uVar2 = 0xffff;
  }
  else {
    uVar2 = 0;
  }
  return uVar2;
}



char * __cdecl16far FUN_2388_078a(char *param_1,int param_2,int *param_3)

{
  char *pcVar1;
  char cVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  char *pcVar7;
  char *pcVar8;
  undefined2 unaff_DS;
  
  if (param_2 < 1) {
LAB_2388_07ea:
    param_1 = (char *)0x0;
  }
  else {
    uVar6 = param_2 - 1;
    pcVar8 = param_1;
    while (uVar6 != 0) {
      uVar4 = param_3[1];
      if (uVar4 == 0) {
        iVar3 = FUN_2388_128e(param_3);
        if (iVar3 == -1) {
          if ((pcVar8 == param_1) || ((*(byte *)(param_3 + 3) & 0x20) != 0)) goto LAB_2388_07ea;
          break;
        }
        pcVar1 = pcVar8;
        pcVar8 = pcVar8 + 1;
        *pcVar1 = (char)iVar3;
        if ((char)iVar3 == '\n') break;
        uVar6 = uVar6 - 1;
      }
      else {
        if (uVar6 < uVar4) {
          uVar4 = uVar6;
        }
        pcVar7 = (char *)*param_3;
        uVar5 = uVar4;
        do {
          pcVar1 = pcVar7;
          pcVar7 = pcVar7 + 1;
          cVar2 = *pcVar1;
          pcVar1 = pcVar8;
          pcVar8 = pcVar8 + 1;
          *pcVar1 = cVar2;
          uVar5 = uVar5 - 1;
        } while (uVar5 != 0 && cVar2 != '\n');
        *param_3 = (int)pcVar7;
        if (cVar2 == '\n') {
          param_3[1] = param_3[1] - (uVar4 - uVar5);
          break;
        }
        param_3[1] = param_3[1] - uVar4;
        uVar6 = uVar6 - uVar4;
      }
    }
    *pcVar8 = '\0';
  }
  return param_1;
}



undefined2 __cdecl16far FUN_2388_07fe(int param_1,uint param_2,int param_3,int param_4)

{
  uint uVar1;
  int in_DX;
  undefined2 unaff_DS;
  bool bVar2;
  long lVar3;
  
  if ((((*(byte *)(param_1 + 6) & 0x83) == 0) || (2 < param_4)) || (param_4 < 0)) {
    *(undefined2 *)0x4568 = 0x16;
  }
  else {
    *(byte *)(param_1 + 6) = *(byte *)(param_1 + 6) & 0xef;
    if (param_4 == 1) {
      uVar1 = FUN_2388_1ef6(param_1);
      bVar2 = CARRY2(param_2,uVar1);
      param_2 = param_2 + uVar1;
      param_3 = param_3 + in_DX + (uint)bVar2;
      param_4 = 0;
    }
    FUN_2388_15ce(param_1);
    if ((*(byte *)(param_1 + 6) & 0x80) != 0) {
      *(byte *)(param_1 + 6) = *(byte *)(param_1 + 6) & 0xfc;
    }
    lVar3 = FUN_2388_1bd2(0x2388,*(undefined1 *)(param_1 + 7),param_2,param_3,param_4);
    if (lVar3 != -1) {
      return 0;
    }
  }
  return 0xffff;
}



void __cdecl16far FUN_2388_087e(undefined2 param_1,undefined2 *param_2)

{
  undefined2 unaff_DS;
  
  FUN_2388_07fe(param_1,*param_2,param_2[1],0);
  return;
}



void __cdecl16far FUN_2388_0898(int param_1)

{
  byte *pbVar1;
  byte bVar2;
  undefined2 unaff_DS;
  
  bVar2 = *(byte *)(param_1 + 7);
  FUN_2388_15ce(param_1);
  pbVar1 = (byte *)(bVar2 + 0x4577);
  *pbVar1 = *pbVar1 & 0xfd;
  *(byte *)(param_1 + 6) = *(byte *)(param_1 + 6) & 0xcf;
  if ((*(byte *)(param_1 + 6) & 0x80) != 0) {
    *(byte *)(param_1 + 6) = *(byte *)(param_1 + 6) & 0xfc;
  }
  FUN_2388_1bd2(0x2388,(uint)bVar2,0,0,0);
  return;
}



void __cdecl16far FUN_2388_08dc(undefined2 param_1,int param_2)

{
  undefined2 uVar1;
  undefined2 uVar2;
  
  if (param_2 == 0) {
    uVar2 = 0;
    uVar1 = 4;
    param_2 = 0;
  }
  else {
    uVar2 = 0x200;
    uVar1 = 0;
  }
  FUN_2388_206c(param_1,param_2,uVar1,uVar2);
  return;
}



undefined2 __cdecl16far FUN_2388_0908(undefined2 param_1,undefined2 param_2)

{
  int *piVar1;
  undefined1 *puVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  
  *(undefined1 *)0x49b0 = 0x42;
  *(undefined2 *)0x49ae = param_1;
  *(undefined2 *)0x49aa = param_1;
  *(undefined2 *)0x49ac = 0x7fff;
  uVar3 = FUN_2388_16a6(0x49aa,param_2,&stack0x0008);
  piVar1 = (int *)0x49ac;
  *piVar1 = *piVar1 + -1;
  if (*piVar1 < 0) {
    FUN_2388_1324(0,0x49aa);
  }
  else {
    puVar2 = (undefined1 *)*(undefined2 *)0x49aa;
    *(int *)0x49aa = *(int *)0x49aa + 1;
    *puVar2 = 0;
  }
  return uVar3;
}



undefined2 __cdecl16far FUN_2388_0962(int param_1)

{
  undefined2 uVar1;
  undefined2 unaff_DS;
  long lVar2;
  long lVar3;
  
  if ((param_1 < 0) || (*(int *)0x4575 <= param_1)) {
    *(undefined2 *)0x4568 = 9;
    uVar1 = 0xffff;
  }
  else {
    lVar2 = FUN_2388_1bd2(0x2388,param_1,0,0,1);
    if (lVar2 == -1) {
      uVar1 = 0xffff;
    }
    else {
      lVar3 = FUN_2388_1bd2(0x2388,param_1,0,0,2);
      uVar1 = (undefined2)lVar3;
      if (lVar3 != lVar2) {
        FUN_2388_1bd2(0x2388,param_1,lVar2,0);
      }
    }
  }
  return uVar1;
}



char * __cdecl16far FUN_2388_09e8(char *param_1,char param_2)

{
  char *pcVar1;
  int iVar2;
  char *pcVar3;
  undefined2 unaff_DS;
  
  iVar2 = -1;
  pcVar3 = param_1;
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    pcVar1 = pcVar3;
    pcVar3 = pcVar3 + 1;
  } while (*pcVar1 != '\0');
  iVar2 = -(iVar2 + 1);
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    pcVar1 = param_1;
    param_1 = param_1 + 1;
  } while (param_2 != *pcVar1);
  param_1 = param_1 + -1;
  if (*param_1 != param_2) {
    param_1 = (char *)0x0;
  }
  return param_1;
}



uint __cdecl16far FUN_2388_0a12(byte *param_1,byte *param_2,int param_3)

{
  byte bVar1;
  byte bVar2;
  uint uVar3;
  undefined2 unaff_DS;
  bool bVar4;
  
  uVar3 = 0;
  if (param_3 != 0) {
    do {
      bVar2 = *param_1;
      bVar1 = *param_2;
      if ((bVar2 == 0) || (bVar1 == 0)) break;
      param_1 = param_1 + 1;
      param_2 = param_2 + 1;
      if ((0x40 < bVar2) && (bVar2 < 0x5b)) {
        bVar2 = bVar2 + 0x20;
      }
      if ((0x40 < bVar1) && (bVar1 < 0x5b)) {
        bVar1 = bVar1 + 0x20;
      }
      bVar4 = bVar2 < bVar1;
      if (bVar2 != bVar1) goto LAB_2388_0a58;
      param_3 = param_3 + -1;
    } while (param_3 != 0);
    uVar3 = 0;
    bVar4 = bVar2 < bVar1;
    if (bVar2 != bVar1) {
LAB_2388_0a58:
      uVar3 = 0;
      if (!bVar4) {
        uVar3 = 0xfffe;
      }
      uVar3 = ~uVar3;
    }
  }
  return uVar3;
}



char * __cdecl16far FUN_2388_0a6a(char *param_1)

{
  char cVar1;
  char *pcVar2;
  undefined2 unaff_DS;
  
  for (pcVar2 = param_1; cVar1 = *pcVar2, cVar1 != '\0'; pcVar2 = pcVar2 + 1) {
    if ((byte)(cVar1 + 0xbfU) < 0x1a) {
      *pcVar2 = cVar1 + ' ';
    }
  }
  return param_1;
}



char * __cdecl16far FUN_2388_0a88(char *param_1)

{
  char cVar1;
  char *pcVar2;
  undefined2 unaff_DS;
  
  for (pcVar2 = param_1; cVar1 = *pcVar2, cVar1 != '\0'; pcVar2 = pcVar2 + 1) {
    if ((byte)(cVar1 + 0x9fU) < 0x1a) {
      *pcVar2 = cVar1 + -0x20;
    }
  }
  return param_1;
}



void FUN_2388_0aa6(void)

{
  code *pcVar1;
  
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  FUN_2388_1238();
  return;
}



void FUN_2388_0ab4(void)

{
  code *pcVar1;
  
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  FUN_2388_1240();
  return;
}



void FUN_2388_0abf(void)

{
  code *pcVar1;
  
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
  FUN_2388_1240();
  return;
}



void FUN_2388_0af2(void)

{
  code *pcVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined1 uVar3;
  undefined2 in_stack_00000000;
  undefined2 *in_stack_0000000c;
  
  uVar3 = *(uint *)0x4896 < 0xd6d6;
  if (*(uint *)0x4896 == 0xd6d6) {
    (*(code *)*(undefined2 *)0x4898)();
  }
  pcVar1 = (code *)swi(0x21);
  uVar2 = (*pcVar1)();
  if (!(bool)uVar3) {
    *in_stack_0000000c = uVar2;
  }
  FUN_2388_1240();
  return;
}



void FUN_2388_0af9(void)

{
  code *pcVar1;
  undefined2 uVar2;
  undefined2 unaff_DS;
  undefined1 uVar3;
  undefined2 in_stack_00000000;
  undefined2 *in_stack_0000000c;
  
  uVar3 = *(uint *)0x4896 < 0xd6d6;
  if (*(uint *)0x4896 == 0xd6d6) {
    (*(code *)*(undefined2 *)0x4898)();
  }
  pcVar1 = (code *)swi(0x21);
  uVar2 = (*pcVar1)();
  if (!(bool)uVar3) {
    *in_stack_0000000c = uVar2;
  }
  FUN_2388_1240();
  return;
}



undefined4 __stdcall16far FUN_2388_0b22(uint param_1,uint param_2,uint param_3,uint param_4)

{
  ulong uVar1;
  long lVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  bool bVar10;
  char cVar11;
  uint uVar9;
  
  cVar11 = (int)param_2 < 0;
  if ((bool)cVar11) {
    bVar10 = param_1 != 0;
    param_1 = -param_1;
    param_2 = -(uint)bVar10 - param_2;
  }
  if ((int)param_4 < 0) {
    cVar11 = cVar11 + '\x01';
    bVar10 = param_3 != 0;
    param_3 = -param_3;
    param_4 = -(uint)bVar10 - param_4;
  }
  uVar3 = param_1;
  uVar5 = param_3;
  uVar6 = param_2;
  uVar9 = param_4;
  if (param_4 == 0) {
    uVar3 = param_2 / param_3;
    iVar4 = (int)(((ulong)param_2 % (ulong)param_3 << 0x10 | (ulong)param_1) / (ulong)param_3);
  }
  else {
    do {
      uVar8 = uVar9 >> 1;
      uVar5 = uVar5 >> 1 | (uint)((uVar9 & 1) != 0) << 0xf;
      uVar7 = uVar6 >> 1;
      uVar3 = uVar3 >> 1 | (uint)((uVar6 & 1) != 0) << 0xf;
      uVar6 = uVar7;
      uVar9 = uVar8;
    } while (uVar8 != 0);
    uVar1 = CONCAT22(uVar7,uVar3) / (ulong)uVar5;
    iVar4 = (int)uVar1;
    lVar2 = (ulong)param_3 * (uVar1 & 0xffff);
    uVar3 = (uint)((ulong)lVar2 >> 0x10);
    uVar5 = uVar3 + iVar4 * param_4;
    if (((CARRY2(uVar3,iVar4 * param_4)) || (param_2 < uVar5)) ||
       ((param_2 <= uVar5 && (param_1 < (uint)lVar2)))) {
      iVar4 = iVar4 + -1;
    }
    uVar3 = 0;
  }
  if (cVar11 == '\x01') {
    bVar10 = iVar4 != 0;
    iVar4 = -iVar4;
    uVar3 = -(uint)bVar10 - uVar3;
  }
  return CONCAT22(uVar3,iVar4);
}



long __stdcall16far FUN_2388_0bbc(uint param_1,int param_2,uint param_3,int param_4)

{
  if (param_4 == 0 && param_2 == 0) {
    return (ulong)param_1 * (ulong)param_3;
  }
  return CONCAT22((int)((ulong)param_1 * (ulong)param_3 >> 0x10) +
                  param_2 * param_3 + param_1 * param_4,(int)((ulong)param_1 * (ulong)param_3));
}



uint __cdecl16far FUN_2388_0bee(byte *param_1,byte *param_2,uint param_3)

{
  byte *pbVar1;
  byte *pbVar2;
  uint uVar3;
  uint uVar4;
  byte *pbVar5;
  byte *pbVar6;
  int iVar7;
  int iVar8;
  bool bVar9;
  bool bVar10;
  
  if (param_3 == 0) {
    return 0;
  }
  iVar8 = (int)((ulong)param_1 >> 0x10);
  pbVar5 = (byte *)param_1;
  iVar7 = (int)((ulong)param_2 >> 0x10);
  pbVar6 = (byte *)param_2;
  do {
    uVar3 = ~(uint)pbVar6;
    uVar3 = ((param_3 - 1) - uVar3 & -(uint)(param_3 - 1 < uVar3)) + uVar3;
    uVar4 = ~(uint)pbVar5;
    uVar3 = (uVar3 - uVar4 & -(uint)(uVar3 < uVar4)) + uVar4 + 1;
    bVar9 = param_3 < uVar3;
    param_3 = param_3 - uVar3;
    bVar10 = param_3 == 0;
    do {
      if (uVar3 == 0) break;
      uVar3 = uVar3 - 1;
      pbVar2 = pbVar6;
      pbVar6 = pbVar6 + 1;
      pbVar1 = pbVar5;
      pbVar5 = pbVar5 + 1;
      bVar9 = *pbVar1 < *pbVar2;
      bVar10 = *pbVar1 == *pbVar2;
    } while (bVar10);
    if (!bVar10) {
      return (1 - (uint)bVar9) - (uint)(bVar9 != 0);
    }
    if (param_3 == 0) {
      return uVar3;
    }
    if (pbVar5 == (byte *)0x0) {
      iVar8 = iVar8 + 0x1000;
    }
    if (pbVar6 == (byte *)0x0) {
      iVar7 = iVar7 + 0x1000;
    }
  } while( true );
}



undefined2 * __cdecl16far FUN_2388_0c4a(undefined2 *param_1,undefined2 *param_2,int param_3)

{
  undefined2 *puVar1;
  undefined2 *puVar2;
  uint uVar3;
  uint uVar4;
  undefined2 *puVar5;
  undefined2 *puVar6;
  int iVar7;
  int iVar8;
  
  if (param_3 != 0) {
    iVar8 = (int)((ulong)param_2 >> 0x10);
    puVar5 = (undefined2 *)param_2;
    iVar7 = (int)((ulong)param_1 >> 0x10);
    puVar6 = (undefined2 *)param_1;
    while( true ) {
      uVar3 = ~(uint)puVar6;
      uVar3 = ((param_3 - 1U) - uVar3 & -(uint)(param_3 - 1U < uVar3)) + uVar3;
      uVar4 = ~(uint)puVar5;
      uVar3 = (uVar3 - uVar4 & -(uint)(uVar3 < uVar4)) + uVar4 + 1;
      param_3 = param_3 - uVar3;
      for (uVar4 = uVar3 >> 1; uVar4 != 0; uVar4 = uVar4 - 1) {
        puVar2 = puVar6;
        puVar6 = puVar6 + 1;
        puVar1 = puVar5;
        puVar5 = puVar5 + 1;
        *puVar2 = *puVar1;
      }
      for (uVar3 = (uint)((uVar3 & 1) != 0); uVar3 != 0; uVar3 = uVar3 - 1) {
        puVar2 = puVar6;
        puVar6 = (undefined2 *)((int)puVar6 + 1);
        puVar1 = puVar5;
        puVar5 = (undefined2 *)((int)puVar5 + 1);
        *(undefined1 *)puVar2 = *(undefined1 *)puVar1;
      }
      if (param_3 == 0) break;
      if (puVar5 == (undefined2 *)0x0) {
        iVar8 = iVar8 + 0x1000;
      }
      if (puVar6 == (undefined2 *)0x0) {
        iVar7 = iVar7 + 0x1000;
      }
    }
  }
  return (undefined2 *)param_1;
}



char * __cdecl16far FUN_2388_0ca8(char *param_1,char param_2)

{
  char *pcVar1;
  int iVar2;
  char *pcVar3;
  char *pcVar4;
  undefined2 uVar5;
  
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  pcVar3 = (char *)param_1;
  iVar2 = -1;
  pcVar4 = pcVar3;
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    pcVar1 = pcVar4;
    pcVar4 = pcVar4 + 1;
  } while (*pcVar1 != '\0');
  iVar2 = -(iVar2 + 1);
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    pcVar1 = pcVar3;
    pcVar3 = pcVar3 + 1;
  } while (param_2 != *pcVar1);
  pcVar3 = pcVar3 + -1;
  if (*pcVar3 != param_2) {
    pcVar3 = (char *)0x0;
  }
  return pcVar3;
}



int __cdecl16far FUN_2388_0cd6(byte *param_1,byte *param_2)

{
  byte *pbVar1;
  byte bVar2;
  byte bVar3;
  char cVar4;
  byte *pbVar5;
  byte *pbVar6;
  
  pbVar6 = (byte *)param_2;
  pbVar5 = (byte *)param_1;
  bVar3 = 0xff;
  do {
    do {
      cVar4 = '\0';
      if (bVar3 == 0) goto LAB_2388_0d15;
      pbVar1 = pbVar6;
      pbVar6 = pbVar6 + 1;
      bVar3 = *pbVar1;
      pbVar1 = pbVar5;
      pbVar5 = pbVar5 + 1;
    } while (*pbVar1 == bVar3);
    bVar2 = bVar3 + 0xbf + (-((byte)(bVar3 + 0xbf) < 0x1a) & 0x20U) + 0x41;
    bVar3 = *pbVar1 + 0xbf;
    bVar3 = bVar3 + (-(bVar3 < 0x1a) & 0x20U) + 0x41;
  } while (bVar3 == bVar2);
  cVar4 = (bVar3 < bVar2) * -2 + '\x01';
LAB_2388_0d15:
  return (int)cVar4;
}



uint __cdecl16far FUN_2388_0d1c(char *param_1,char *param_2,int param_3)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  uint uVar4;
  char *pcVar5;
  char *pcVar6;
  undefined2 uVar7;
  undefined2 uVar8;
  
  uVar4 = 0;
  if (param_3 != 0) {
    uVar7 = (undefined2)((ulong)param_1 >> 0x10);
    pcVar6 = (char *)param_1;
    iVar3 = param_3;
    pcVar5 = pcVar6;
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      pcVar1 = pcVar5;
      pcVar5 = pcVar5 + 1;
    } while (*pcVar1 != '\0');
    param_3 = param_3 - iVar3;
    uVar8 = (undefined2)((ulong)param_2 >> 0x10);
    pcVar5 = (char *)param_2;
    do {
      if (param_3 == 0) break;
      param_3 = param_3 + -1;
      pcVar2 = pcVar6;
      pcVar6 = pcVar6 + 1;
      pcVar1 = pcVar5;
      pcVar5 = pcVar5 + 1;
    } while (*pcVar1 == *pcVar2);
    uVar4 = 0;
    if ((byte)pcVar5[-1] <= (byte)pcVar6[-1]) {
      if (pcVar5[-1] == pcVar6[-1]) {
        return 0;
      }
      uVar4 = 0xfffe;
    }
    uVar4 = ~uVar4;
  }
  return uVar4;
}



char * __cdecl16far FUN_2388_0d58(char *param_1,char *param_2,int param_3)

{
  char *pcVar1;
  char *pcVar2;
  char *pcVar3;
  char *pcVar4;
  undefined2 uVar5;
  
  uVar5 = (undefined2)((ulong)param_1 >> 0x10);
  pcVar3 = (char *)param_2;
  pcVar4 = (char *)param_1;
  if (param_3 != 0) {
    do {
      pcVar1 = pcVar3;
      pcVar3 = pcVar3 + 1;
      if (*pcVar1 == '\0') break;
      pcVar2 = pcVar4;
      pcVar4 = pcVar4 + 1;
      *pcVar2 = *pcVar1;
      param_3 = param_3 + -1;
    } while (param_3 != 0);
    for (; param_3 != 0; param_3 = param_3 + -1) {
      pcVar1 = pcVar4;
      pcVar4 = pcVar4 + 1;
      *pcVar1 = '\0';
    }
  }
  return (char *)param_1;
}



char * __cdecl16far FUN_2388_0d82(char *param_1,char param_2)

{
  char *pcVar1;
  int iVar2;
  char *pcVar3;
  undefined2 uVar4;
  
  uVar4 = (undefined2)((ulong)param_1 >> 0x10);
  pcVar3 = (char *)param_1;
  iVar2 = -1;
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    pcVar1 = pcVar3;
    pcVar3 = pcVar3 + 1;
  } while (*pcVar1 != '\0');
  iVar2 = -(iVar2 + 1);
  pcVar3 = pcVar3 + -1;
  do {
    if (iVar2 == 0) break;
    iVar2 = iVar2 + -1;
    pcVar1 = pcVar3;
    pcVar3 = pcVar3 + -1;
  } while (param_2 != *pcVar1);
  pcVar3 = pcVar3 + 1;
  if (*pcVar3 != param_2) {
    pcVar3 = (char *)0x0;
  }
  return pcVar3;
}



char * __cdecl16far FUN_2388_0db0(char *param_1)

{
  char cVar1;
  char *pcVar2;
  undefined2 uVar3;
  
  uVar3 = (undefined2)((ulong)param_1 >> 0x10);
  for (pcVar2 = (char *)param_1; cVar1 = *pcVar2, cVar1 != '\0'; pcVar2 = pcVar2 + 1) {
    if ((byte)(cVar1 + 0x9fU) < 0x1a) {
      *pcVar2 = cVar1 + -0x20;
    }
  }
  return (char *)param_1;
}



int __cdecl16far FUN_2388_0dd4(char *param_1)

{
  char *pcVar1;
  uint uVar2;
  char *pcVar3;
  
  pcVar3 = (char *)param_1;
  uVar2 = 0xffff;
  do {
    if (uVar2 == 0) break;
    uVar2 = uVar2 - 1;
    pcVar1 = pcVar3;
    pcVar3 = pcVar3 + 1;
  } while (*pcVar1 != '\0');
  return ~uVar2 - 1;
}



void __cdecl16far FUN_2388_0dec(char *param_1,char *param_2)

{
  char *pcVar1;
  char *pcVar2;
  uint uVar3;
  uint uVar4;
  char *pcVar5;
  char *pcVar6;
  undefined2 uVar7;
  undefined2 uVar8;
  
  uVar8 = (undefined2)((ulong)param_2 >> 0x10);
  pcVar5 = (char *)param_2;
  uVar3 = 0xffff;
  pcVar6 = pcVar5;
  do {
    if (uVar3 == 0) break;
    uVar3 = uVar3 - 1;
    pcVar1 = pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (*pcVar1 != '\0');
  uVar3 = ~uVar3;
  uVar7 = (undefined2)((ulong)param_1 >> 0x10);
  pcVar6 = (char *)param_1;
  if (((ulong)param_1 & 1) != 0) {
    pcVar6 = pcVar6 + 1;
    pcVar5 = pcVar5 + 1;
    *param_1 = *param_2;
    uVar3 = uVar3 - 1;
  }
  for (uVar4 = uVar3 >> 1; uVar4 != 0; uVar4 = uVar4 - 1) {
    pcVar2 = pcVar6;
    pcVar6 = pcVar6 + 2;
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 2;
    *(undefined2 *)pcVar2 = *(undefined2 *)pcVar1;
  }
  for (uVar3 = (uint)((uVar3 & 1) != 0); uVar3 != 0; uVar3 = uVar3 - 1) {
    pcVar2 = pcVar6;
    pcVar6 = pcVar6 + 1;
    pcVar1 = pcVar5;
    pcVar5 = pcVar5 + 1;
    *pcVar2 = *pcVar1;
  }
  return;
}



char * __cdecl16far FUN_2388_0e22(char *param_1,char *param_2)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  char *pcVar6;
  char *pcVar7;
  char *pcVar8;
  undefined2 uVar9;
  undefined2 uVar10;
  
  uVar9 = (undefined2)((ulong)param_1 >> 0x10);
  iVar3 = -1;
  pcVar6 = (char *)param_1;
  do {
    if (iVar3 == 0) break;
    iVar3 = iVar3 + -1;
    pcVar1 = pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (*pcVar1 != '\0');
  uVar10 = (undefined2)((ulong)param_2 >> 0x10);
  pcVar7 = (char *)param_2;
  uVar4 = 0xffff;
  do {
    if (uVar4 == 0) break;
    uVar4 = uVar4 - 1;
    pcVar1 = pcVar7;
    pcVar7 = pcVar7 + 1;
  } while (*pcVar1 != '\0');
  uVar4 = ~uVar4;
  pcVar7 = pcVar7 + -uVar4;
  pcVar8 = pcVar6 + -1;
  if (((uint)pcVar7 & 1) != 0) {
    pcVar1 = pcVar7;
    pcVar7 = pcVar7 + 1;
    pcVar6[-1] = *pcVar1;
    uVar4 = uVar4 - 1;
    pcVar8 = pcVar6;
  }
  for (uVar5 = uVar4 >> 1; uVar5 != 0; uVar5 = uVar5 - 1) {
    pcVar2 = pcVar8;
    pcVar8 = pcVar8 + 2;
    pcVar1 = pcVar7;
    pcVar7 = pcVar7 + 2;
    *(undefined2 *)pcVar2 = *(undefined2 *)pcVar1;
  }
  for (uVar4 = (uint)((uVar4 & 1) != 0); uVar4 != 0; uVar4 = uVar4 - 1) {
    pcVar2 = pcVar8;
    pcVar8 = pcVar8 + 1;
    pcVar1 = pcVar7;
    pcVar7 = pcVar7 + 1;
    *pcVar2 = *pcVar1;
  }
  return (char *)param_1;
}



undefined2 * __cdecl16far FUN_2388_0e68(undefined2 *param_1,undefined1 param_2,uint param_3)

{
  undefined2 *puVar1;
  uint uVar2;
  uint uVar3;
  uint uVar4;
  undefined2 *puVar5;
  int iVar6;
  
  if (param_3 != 0) {
    iVar6 = (int)((ulong)param_1 >> 0x10);
    uVar2 = -(int)(undefined2 *)param_1;
    uVar4 = 0;
    uVar3 = param_3;
    if (uVar2 != 0) {
      uVar3 = (uVar2 - param_3 & -(uint)(uVar2 < param_3)) + param_3;
      uVar4 = param_3 - uVar3;
    }
    puVar5 = (undefined2 *)param_1;
    for (uVar2 = uVar3 >> 1; uVar2 != 0; uVar2 = uVar2 - 1) {
      puVar1 = puVar5;
      puVar5 = puVar5 + 1;
      *puVar1 = CONCAT11(param_2,param_2);
    }
    for (uVar3 = (uint)((uVar3 & 1) != 0); uVar3 != 0; uVar3 = uVar3 - 1) {
      puVar1 = puVar5;
      puVar5 = (undefined2 *)((int)puVar5 + 1);
      *(undefined1 *)puVar1 = param_2;
    }
    if (uVar4 != 0) {
      for (uVar3 = uVar4 >> 1; uVar3 != 0; uVar3 = uVar3 - 1) {
        puVar1 = puVar5;
        puVar5 = puVar5 + 1;
        *puVar1 = CONCAT11(param_2,param_2);
      }
      for (uVar4 = (uint)((uVar4 & 1) != 0); uVar4 != 0; uVar4 = uVar4 - 1) {
        puVar1 = puVar5;
        puVar5 = (undefined2 *)((int)puVar5 + 1);
        *(undefined1 *)puVar1 = param_2;
      }
    }
  }
  return (undefined2 *)param_1;
}



undefined2 * __cdecl16far FUN_2388_0eb0(undefined2 *param_1,undefined2 *param_2,uint param_3)

{
  undefined1 *puVar1;
  undefined2 *puVar2;
  undefined1 *puVar3;
  undefined2 *puVar4;
  undefined1 *puVar5;
  uint uVar6;
  uint uVar7;
  undefined2 *puVar8;
  undefined1 *puVar9;
  undefined2 *puVar10;
  undefined1 *puVar11;
  undefined2 *puVar12;
  int iVar13;
  int iVar14;
  long lVar15;
  
  puVar10 = (undefined2 *)param_1;
  if (param_3 != 0) {
    iVar14 = (int)((ulong)param_2 >> 0x10);
    puVar8 = (undefined2 *)param_2;
    iVar13 = (int)((ulong)param_1 >> 0x10);
    lVar15 = FUN_2388_1eca(puVar10,iVar13,puVar8,iVar14);
    puVar12 = puVar10;
    if ((lVar15 < 0) || ((uint)((uint)lVar15 < param_3) <= (uint)((ulong)lVar15 >> 0x10))) {
      while( true ) {
        uVar6 = ~(uint)puVar12;
        uVar6 = ((param_3 - 1) - uVar6 & -(uint)(param_3 - 1 < uVar6)) + uVar6;
        uVar7 = ~(uint)puVar8;
        uVar6 = (uVar6 - uVar7 & -(uint)(uVar6 < uVar7)) + uVar7 + 1;
        param_3 = param_3 - uVar6;
        for (uVar7 = uVar6 >> 1; uVar7 != 0; uVar7 = uVar7 - 1) {
          puVar4 = puVar12;
          puVar12 = puVar12 + 1;
          puVar2 = puVar8;
          puVar8 = puVar8 + 1;
          *puVar4 = *puVar2;
        }
        for (uVar6 = (uint)((uVar6 & 1) != 0); uVar6 != 0; uVar6 = uVar6 - 1) {
          puVar4 = puVar12;
          puVar12 = (undefined2 *)((int)puVar12 + 1);
          puVar2 = puVar8;
          puVar8 = (undefined2 *)((int)puVar8 + 1);
          *(undefined1 *)puVar4 = *(undefined1 *)puVar2;
        }
        if (param_3 == 0) break;
        if (puVar8 == (undefined2 *)0x0) {
          iVar14 = iVar14 + 0x1000;
        }
        if (puVar12 == (undefined2 *)0x0) {
          iVar13 = iVar13 + 0x1000;
        }
      }
    }
    else {
      uVar6 = param_3 - 1;
      puVar9 = (undefined1 *)((int)puVar8 + uVar6);
      if (CARRY2((uint)puVar8,uVar6)) {
        iVar14 = iVar14 + 0x1000;
      }
      puVar11 = (undefined1 *)((int)puVar10 + uVar6);
      if (CARRY2((uint)puVar10,uVar6)) {
        iVar13 = iVar13 + 0x1000;
      }
      while( true ) {
        puVar5 = puVar9 + ((int)(puVar11 +
                                ((int)(param_3 - 1) - (int)puVar11 &
                                -(uint)((undefined1 *)(param_3 - 1) < puVar11))) - (int)puVar9 &
                          -(uint)(puVar11 + ((int)(param_3 - 1) - (int)puVar11 &
                                            -(uint)((undefined1 *)(param_3 - 1) < puVar11)) < puVar9
                                 )) + 1;
        param_3 = param_3 - (int)puVar5;
        for (; puVar5 != (undefined1 *)0x0; puVar5 = puVar5 + -1) {
          puVar3 = puVar11;
          puVar11 = puVar11 + -1;
          puVar1 = puVar9;
          puVar9 = puVar9 + -1;
          *puVar3 = *puVar1;
        }
        if (param_3 == 0) break;
        if (puVar9 == (undefined1 *)0xffff) {
          iVar14 = iVar14 + -0x1000;
        }
        if (puVar11 == (undefined1 *)0xffff) {
          iVar13 = iVar13 + -0x1000;
        }
      }
    }
  }
  return puVar10;
}



void __cdecl16far FUN_2388_0f7a(void)

{
  undefined2 unaff_DS;
  
  FUN_2388_1201(0xfc);
  if (*(int *)0x46ac != 0) {
    (*(code *)*(undefined2 *)0x46aa)(0x2388);
  }
  FUN_2388_1201(0xff);
  return;
}



void FUN_2388_0f9c(void)

{
  FUN_2388_00f1();
  return;
}



uint __cdecl16far FUN_2388_0fa2(void)

{
  byte *pbVar1;
  byte bVar3;
  uint uVar2;
  int iVar4;
  byte *pbVar5;
  undefined2 unaff_DS;
  
  pbVar5 = (byte *)0x0;
  iVar4 = 0x42;
  bVar3 = 0;
  do {
    pbVar1 = pbVar5;
    pbVar5 = pbVar5 + 1;
    bVar3 = bVar3 ^ *pbVar1;
    iVar4 = iVar4 + -1;
  } while (iVar4 != 0);
  uVar2 = CONCAT11(bVar3,*pbVar1) ^ 0x5500;
  if (bVar3 != 0x55) {
    FUN_2388_0f7a();
    FUN_2388_1201(1);
    uVar2 = 1;
  }
  return uVar2;
}



// WARNING (jumptable): Unable to track spacebase fully for stack
// WARNING: Unable to track spacebase fully for stack

void FUN_2388_0fc6(undefined2 param_1,undefined2 param_2,undefined2 param_3)

{
  char *pcVar1;
  char cVar2;
  char *pcVar3;
  undefined2 uVar4;
  code *pcVar5;
  undefined2 uVar6;
  int iVar7;
  uint uVar8;
  uint uVar9;
  undefined2 *puVar10;
  char *pcVar11;
  char *pcVar12;
  char *pcVar13;
  int iVar14;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  undefined2 in_stack_00000000;
  
  *(undefined2 *)0x46ae = in_stack_00000000;
  *(undefined2 *)0x46b0 = param_1;
  pcVar5 = (code *)swi(0x21);
  uVar6 = (*pcVar5)();
  *(undefined2 *)0x4570 = uVar6;
  uVar9 = 1;
  if ((char)uVar6 != '\x02') {
    uVar6 = *(undefined2 *)0x2c;
    *(undefined2 *)0x4593 = uVar6;
    iVar7 = -0x8000;
    pcVar12 = (char *)0x0;
LAB_2388_0ff1:
    do {
      pcVar13 = pcVar12;
      if (iVar7 != 0) {
        iVar7 = iVar7 + -1;
        pcVar3 = pcVar12;
        pcVar12 = pcVar12 + 1;
        pcVar13 = pcVar12;
        if (*pcVar3 != '\0') goto LAB_2388_0ff1;
      }
      pcVar12 = pcVar13 + 1;
    } while (*pcVar13 != '\0');
    pcVar13 = pcVar13 + 3;
    *(undefined2 *)0x4591 = pcVar13;
    uVar9 = 0xffff;
    do {
      if (uVar9 == 0) break;
      uVar9 = uVar9 - 1;
      pcVar3 = pcVar13;
      pcVar13 = pcVar13 + 1;
    } while (*pcVar3 != '\0');
    uVar9 = ~uVar9;
  }
  iVar7 = 1;
  pcVar12 = (char *)0x81;
  uVar6 = *(undefined2 *)0x456e;
LAB_2388_100f:
  do {
    do {
      pcVar3 = pcVar12;
      pcVar12 = pcVar12 + 1;
      cVar2 = *pcVar3;
    } while (cVar2 == ' ');
  } while (cVar2 == '\t');
  if ((cVar2 != '\r') && (cVar2 != '\0')) {
    iVar7 = iVar7 + 1;
    do {
      pcVar12 = pcVar12 + -1;
LAB_2388_1022:
      pcVar3 = pcVar12;
      pcVar12 = pcVar12 + 1;
      cVar2 = *pcVar3;
      if ((cVar2 == ' ') || (cVar2 == '\t')) goto LAB_2388_100f;
      if ((cVar2 == '\r') || (cVar2 == '\0')) break;
      if (cVar2 == '\"') {
LAB_2388_105b:
        do {
          while( true ) {
            while( true ) {
              pcVar3 = pcVar12;
              pcVar12 = pcVar12 + 1;
              cVar2 = *pcVar3;
              if ((cVar2 == '\r') || (cVar2 == '\0')) goto LAB_2388_108b;
              if (cVar2 == '\"') goto LAB_2388_1022;
              if (cVar2 == '\\') break;
              uVar9 = uVar9 + 1;
            }
            uVar8 = 0;
            do {
              pcVar13 = pcVar12;
              uVar8 = uVar8 + 1;
              pcVar12 = pcVar13 + 1;
            } while (*pcVar13 == '\\');
            if (*pcVar13 == '\"') break;
            uVar9 = uVar9 + uVar8;
            pcVar12 = pcVar13;
          }
          uVar9 = uVar9 + (uVar8 >> 1) + (uint)((uVar8 & 1) != 0);
        } while ((uVar8 & 1) != 0);
        goto LAB_2388_1022;
      }
      if (cVar2 != '\\') {
        uVar9 = uVar9 + 1;
        goto LAB_2388_1022;
      }
      uVar8 = 0;
      do {
        uVar8 = uVar8 + 1;
        pcVar3 = pcVar12;
        pcVar12 = pcVar12 + 1;
      } while (*pcVar3 == '\\');
      if (*pcVar3 == '\"') {
        uVar9 = uVar9 + (uVar8 >> 1) + (uint)((uVar8 & 1) != 0);
        if ((uVar8 & 1) == 0) goto LAB_2388_105b;
        goto LAB_2388_1022;
      }
      uVar9 = uVar9 + uVar8;
    } while( true );
  }
LAB_2388_108b:
  *(int *)0x458b = iVar7;
  iVar14 = (iVar7 + 1) * 2;
  iVar7 = -(uVar9 + iVar7 + iVar14 + 1 & 0xfffe);
  *(undefined1 **)0x458d = &stack0x0008 + iVar7;
  pcVar13 = &stack0x0008 + iVar14 + iVar7;
  *(undefined2 *)((int)&stack0x0006 + iVar7) = unaff_SS;
  uVar6 = *(undefined2 *)((int)&stack0x0006 + iVar7);
  *(char **)(&stack0x0008 + iVar7) = pcVar13;
  puVar10 = (undefined2 *)(&stack0x000a + iVar7);
  pcVar3 = (char *)*(undefined4 *)0x4591;
  pcVar12 = (char *)pcVar3;
  do {
    pcVar1 = pcVar12;
    pcVar12 = pcVar12 + 1;
    cVar2 = *pcVar1;
    pcVar1 = pcVar13;
    pcVar13 = pcVar13 + 1;
    *pcVar1 = cVar2;
  } while (cVar2 != '\0');
  uVar4 = *(undefined2 *)0x456e;
  pcVar12 = (char *)0x81;
LAB_2388_10c5:
  do {
    do {
      pcVar3 = pcVar12;
      pcVar12 = pcVar12 + 1;
      cVar2 = *pcVar3;
    } while (cVar2 == ' ');
  } while (cVar2 == '\t');
  if ((cVar2 == '\r') || (cVar2 == '\0')) {
LAB_2388_114e:
    *(undefined2 *)((int)&stack0x0006 + iVar7) = unaff_SS;
    uVar6 = *(undefined2 *)((int)&stack0x0006 + iVar7);
    *puVar10 = 0;
                    // WARNING: Could not recover jumptable at 0x000249d4. Too many branches
                    // WARNING: Treating indirect jump as call
    (*(code *)(ulong)*(uint *)0x46ae)();
    return;
  }
  *puVar10 = pcVar13;
  puVar10 = puVar10 + 1;
  do {
    pcVar12 = pcVar12 + -1;
LAB_2388_10dc:
    pcVar3 = pcVar12;
    pcVar12 = pcVar12 + 1;
    cVar2 = *pcVar3;
    if ((cVar2 == ' ') || (cVar2 == '\t')) {
      pcVar3 = pcVar13;
      pcVar13 = pcVar13 + 1;
      *pcVar3 = '\0';
      goto LAB_2388_10c5;
    }
    if ((cVar2 == '\r') || (cVar2 == '\0')) {
LAB_2388_114b:
      *pcVar13 = '\0';
      goto LAB_2388_114e;
    }
    pcVar11 = pcVar12;
    if (cVar2 == '\"') {
LAB_2388_1118:
      while( true ) {
        pcVar12 = pcVar11 + 1;
        cVar2 = *pcVar11;
        if ((cVar2 == '\r') || (cVar2 == '\0')) goto LAB_2388_114b;
        if (cVar2 == '\"') break;
        if (cVar2 == '\\') {
          uVar9 = 0;
          do {
            pcVar11 = pcVar12;
            uVar9 = uVar9 + 1;
            pcVar12 = pcVar11 + 1;
          } while (*pcVar11 == '\\');
          if (*pcVar11 == '\"') {
            for (uVar8 = uVar9 >> 1; uVar8 != 0; uVar8 = uVar8 - 1) {
              pcVar3 = pcVar13;
              pcVar13 = pcVar13 + 1;
              *pcVar3 = '\\';
            }
            if ((uVar9 & 1) == 0) break;
            pcVar3 = pcVar13;
            pcVar13 = pcVar13 + 1;
            *pcVar3 = '\"';
            pcVar11 = pcVar12;
          }
          else {
            for (; uVar9 != 0; uVar9 = uVar9 - 1) {
              pcVar3 = pcVar13;
              pcVar13 = pcVar13 + 1;
              *pcVar3 = '\\';
            }
          }
        }
        else {
          pcVar3 = pcVar13;
          pcVar13 = pcVar13 + 1;
          *pcVar3 = cVar2;
          pcVar11 = pcVar12;
        }
      }
      goto LAB_2388_10dc;
    }
    if (cVar2 != '\\') {
      pcVar3 = pcVar13;
      pcVar13 = pcVar13 + 1;
      *pcVar3 = cVar2;
      goto LAB_2388_10dc;
    }
    uVar9 = 0;
    do {
      uVar9 = uVar9 + 1;
      pcVar3 = pcVar12;
      pcVar12 = pcVar12 + 1;
    } while (*pcVar3 == '\\');
    if (*pcVar3 == '\"') {
      for (uVar8 = uVar9 >> 1; uVar8 != 0; uVar8 = uVar8 - 1) {
        pcVar3 = pcVar13;
        pcVar13 = pcVar13 + 1;
        *pcVar3 = '\\';
      }
      pcVar11 = pcVar12;
      if ((uVar9 & 1) == 0) goto LAB_2388_1118;
      pcVar3 = pcVar13;
      pcVar13 = pcVar13 + 1;
      *pcVar3 = '\"';
      goto LAB_2388_10dc;
    }
    for (; uVar9 != 0; uVar9 = uVar9 - 1) {
      pcVar3 = pcVar13;
      pcVar13 = pcVar13 + 1;
      *pcVar3 = '\\';
    }
  } while( true );
}



void __cdecl16far FUN_2388_1158(void)

{
  int *piVar1;
  char *pcVar2;
  int *piVar3;
  char cVar4;
  int iVar5;
  undefined2 *puVar6;
  int iVar7;
  int iVar8;
  int *piVar9;
  int *piVar10;
  char *pcVar11;
  int *piVar12;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar13;
  
  iVar5 = *(int *)0x2c;
  iVar8 = 0;
  pcVar11 = (char *)0x0;
  iVar7 = -1;
  if (iVar5 != 0) {
    cVar4 = *(char *)0x0;
    while (cVar4 != '\0') {
      do {
        if (iVar7 == 0) break;
        iVar7 = iVar7 + -1;
        pcVar2 = pcVar11;
        pcVar11 = pcVar11 + 1;
      } while (*pcVar2 != '\0');
      iVar8 = iVar8 + 1;
      pcVar2 = pcVar11;
      pcVar11 = pcVar11 + 1;
      cVar4 = *pcVar2;
    }
  }
  pcVar11 = (char *)FUN_2388_21b8();
  puVar6 = (undefined2 *)FUN_2388_21b8();
  *(undefined2 *)0x458f = puVar6;
  piVar9 = (int *)0x0;
  do {
    if (iVar8 == 0) {
      *puVar6 = 0;
      return;
    }
    bVar13 = *piVar9 == *(int *)0x454c;
    if (bVar13) {
      piVar12 = (int *)0x454c;
      iVar7 = 6;
      piVar10 = piVar9;
      do {
        if (iVar7 == 0) break;
        iVar7 = iVar7 + -1;
        piVar3 = piVar12;
        piVar12 = piVar12 + 1;
        piVar1 = piVar10;
        piVar10 = piVar10 + 1;
        bVar13 = *piVar1 == *piVar3;
      } while (bVar13);
      if (!bVar13) goto LAB_2388_11c2;
    }
    else {
LAB_2388_11c2:
      *puVar6 = pcVar11;
      puVar6 = puVar6 + 1;
    }
    do {
      piVar1 = piVar9;
      piVar9 = (int *)((int)piVar9 + 1);
      iVar7 = *piVar1;
      pcVar2 = pcVar11;
      pcVar11 = pcVar11 + 1;
      *pcVar2 = (char)iVar7;
    } while ((char)iVar7 != '\0');
    iVar8 = iVar8 + -1;
  } while( true );
}



int * __stdcall16far FUN_2388_11d6(int param_1)

{
  int *piVar1;
  int iVar2;
  int *piVar3;
  int *piVar4;
  undefined2 unaff_DS;
  
  piVar3 = (int *)0x48c0;
  do {
    piVar1 = piVar3;
    piVar3 = piVar3 + 1;
    piVar4 = piVar3;
    if ((*piVar1 == param_1) || (piVar4 = (int *)0x0, *piVar1 == -1)) {
      return piVar4;
    }
    iVar2 = -1;
    do {
      if (iVar2 == 0) break;
      iVar2 = iVar2 + -1;
      piVar1 = piVar3;
      piVar3 = (int *)((int)piVar3 + 1);
    } while ((char)*piVar1 != '\0');
  } while( true );
}



void __stdcall16far FUN_2388_1201(undefined2 param_1)

{
  char *pcVar1;
  code *pcVar2;
  char *pcVar3;
  int iVar4;
  undefined2 unaff_ES;
  undefined2 unaff_DS;
  
  pcVar3 = (char *)FUN_2388_11d6(param_1);
  if (pcVar3 != (char *)0x0) {
    iVar4 = -1;
    do {
      if (iVar4 == 0) break;
      iVar4 = iVar4 + -1;
      pcVar1 = pcVar3;
      pcVar3 = pcVar3 + 1;
    } while (*pcVar1 != '\0');
    if (*(int *)0x4896 == -0x292a) {
      (*(code *)*(undefined2 *)0x4898)();
    }
    pcVar2 = (code *)swi(0x21);
    (*pcVar2)();
  }
  return;
}



undefined2 __cdecl16far FUN_2388_1238(void)

{
  bool in_CF;
  
  if (!in_CF) {
    return 0;
  }
  FUN_2388_1260();
  return 0xffff;
}



undefined1 __cdecl16far FUN_2388_1240(void)

{
  undefined1 in_AL;
  bool in_CF;
  
  if (in_CF) {
    FUN_2388_1260();
    return in_AL;
  }
  return 0;
}



void __cdecl16far FUN_2388_124d(void)

{
  bool in_CF;
  
  if (in_CF) {
    FUN_2388_1260();
  }
  return;
}



void __cdecl16near FUN_2388_1260(void)

{
  byte bVar1;
  char cVar2;
  uint in_AX;
  undefined2 unaff_DS;
  
  bVar1 = (byte)in_AX;
  *(byte *)0x4573 = bVar1;
  cVar2 = (char)(in_AX >> 8);
  if (cVar2 != '\0') goto LAB_2388_1284;
  if (*(byte *)0x4570 < 3) {
LAB_2388_127a:
    if (0x13 < bVar1) {
LAB_2388_127e:
      in_AX = 0x13;
    }
  }
  else {
    if (0x21 < bVar1) goto LAB_2388_127e;
    if (bVar1 < 0x20) goto LAB_2388_127a;
    in_AX = 5;
  }
  cVar2 = *(char *)(ulong)((in_AX & 0xff) + 0x46b2);
LAB_2388_1284:
  *(int *)0x4568 = (int)cVar2;
  return;
}



uint __cdecl16far FUN_2388_128e(undefined2 *param_1)

{
  byte *pbVar1;
  byte bVar2;
  undefined2 uVar3;
  int iVar4;
  uint uVar5;
  undefined2 unaff_DS;
  
  bVar2 = *(byte *)(param_1 + 3);
  if (((bVar2 & 0x83) != 0) && ((bVar2 & 0x40) == 0)) {
    if ((bVar2 & 2) == 0) {
      *(byte *)(param_1 + 3) = bVar2 | 1;
      if (((bVar2 & 0xc) == 0) && ((*(byte *)(param_1 + 0x50) & 1) == 0)) {
        FUN_2388_21de(param_1);
      }
      uVar3 = param_1[2];
      *param_1 = uVar3;
      uVar5 = (uint)*(byte *)((int)param_1 + 7);
      iVar4 = FUN_2388_1c4c(0x2388,uVar5,uVar3,param_1[0x51]);
      if (iVar4 == 0) {
        *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 0x10;
      }
      else {
        if (iVar4 != -1) {
          if (((*(byte *)(uVar5 + 0x4577) & 0x82) == 0x82) && ((*(byte *)(param_1 + 3) & 0x82) == 0)
             ) {
            pbVar1 = (byte *)(param_1 + 0x50);
            *pbVar1 = *pbVar1 | 0x20;
          }
          param_1[1] = iVar4 + -1;
          bVar2 = *(byte *)*param_1;
          *param_1 = (byte *)*param_1 + 1;
          return (uint)bVar2;
        }
        *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 0x20;
      }
      param_1[1] = 0;
    }
    else {
      *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 0x20;
    }
  }
  return 0xffff;
}



uint __cdecl16far FUN_2388_1324(uint param_1,int *param_2)

{
  int *piVar1;
  byte bVar2;
  int iVar3;
  int iVar4;
  uint uVar5;
  undefined2 unaff_DS;
  
  piVar1 = param_2;
  bVar2 = *(byte *)(param_2 + 3);
  if (((bVar2 & 0x82) != 0) && ((bVar2 & 0x40) == 0)) {
    param_2[1] = 0;
    if ((bVar2 & 1) != 0) {
      if ((bVar2 & 0x10) == 0) goto LAB_2388_139c;
      *param_2 = param_2[2];
      bVar2 = bVar2 & 0xfe;
    }
    *(byte *)(param_2 + 3) = bVar2 & 0xef | 2;
    uVar5 = (uint)*(byte *)((int)param_2 + 7);
    if (((bVar2 & 8) == 0) &&
       (((bVar2 & 4) != 0 ||
        (((*(byte *)(param_2 + 0x50) & 1) == 0 &&
         (((((param_2 == (int *)0x46ce || (param_2 == (int *)0x46d6)) || (param_2 == (int *)0x46e6))
           && ((*(byte *)(uVar5 + 0x4577) & 0x40) != 0)) ||
          (FUN_2388_21de(param_2), (*(byte *)(piVar1 + 3) & 8) == 0)))))))) {
      iVar3 = FUN_2388_1d36(0x2388,uVar5,&param_1,1);
      iVar4 = 1;
    }
    else {
      iVar4 = *piVar1 - piVar1[2];
      *piVar1 = piVar1[2] + 1;
      piVar1[1] = piVar1[0x51] + -1;
      if (iVar4 == 0) {
        iVar3 = 0;
        if ((*(byte *)(uVar5 + 0x4577) & 0x20) != 0) {
          FUN_2388_1bd2(0x2388,uVar5,0,0,2);
          iVar3 = 0;
          iVar4 = 0;
        }
      }
      else {
        iVar3 = FUN_2388_1d36(0x2388,uVar5,piVar1[2],iVar4);
      }
      *(undefined1 *)piVar1[2] = (char)param_1;
    }
    if (iVar3 == iVar4) {
      return param_1 & 0xff;
    }
  }
LAB_2388_139c:
  *(byte *)(piVar1 + 3) = *(byte *)(piVar1 + 3) | 0x20;
  return 0xffff;
}



void __cdecl16near FUN_2388_1408(undefined2 *param_1)

{
  undefined2 unaff_DS;
  
  if (((*(byte *)(param_1 + 3) & 0x83) != 0) && ((*(byte *)(param_1 + 3) & 8) != 0)) {
    thunk_FUN_2388_23fe(param_1[2]);
    *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) & 0xf7;
    param_1[2] = 0;
    *param_1 = 0;
    param_1[1] = 0;
  }
  return;
}



undefined2 * __cdecl16far
FUN_2388_1434(undefined2 param_1,byte *param_2,undefined2 param_3,undefined2 *param_4)

{
  byte bVar1;
  bool bVar2;
  int iVar3;
  uint uVar4;
  undefined2 unaff_DS;
  undefined1 local_8;
  undefined1 local_6;
  
  bVar1 = *param_2;
  if (bVar1 == 0x77) {
    uVar4 = 0x301;
  }
  else {
    if (0x77 < bVar1) {
      return (undefined2 *)0x0;
    }
    if (bVar1 != 0x61) {
      if (bVar1 != 0x72) {
        return (undefined2 *)0x0;
      }
      uVar4 = 0;
      local_6 = 1;
      goto LAB_2388_145c;
    }
    uVar4 = 0x109;
  }
  local_6 = 2;
LAB_2388_145c:
  bVar2 = true;
  do {
    while( true ) {
      param_2 = param_2 + 1;
      if ((*param_2 == 0) || (!bVar2)) {
        iVar3 = FUN_2388_2222(param_1,uVar4,param_3,0x1a4);
        if (iVar3 < 0) {
          return (undefined2 *)0x0;
        }
        *(int *)0x4870 = *(int *)0x4870 + 1;
        *(undefined1 *)(param_4 + 3) = local_6;
        *(undefined1 *)(param_4 + 0x50) = 0;
        param_4[1] = 0;
        param_4[0x52] = 0;
        *param_4 = 0;
        param_4[2] = 0;
        local_8 = (undefined1)iVar3;
        *(undefined1 *)((int)param_4 + 7) = local_8;
        return param_4;
      }
      bVar1 = *param_2;
      if (bVar1 != 0x74) break;
      if ((uVar4 & 0xc000) == 0) {
        uVar4 = uVar4 | 0x4000;
      }
      else {
LAB_2388_1484:
        bVar2 = false;
      }
    }
    if (0x74 < bVar1) goto LAB_2388_1484;
    if (bVar1 == 0x2b) {
      if ((uVar4 & 2) != 0) goto LAB_2388_1484;
      uVar4 = uVar4 & 0xfffe | 2;
      local_6 = 0x80;
    }
    else {
      if ((bVar1 != 0x62) || ((uVar4 & 0xc000) != 0)) goto LAB_2388_1484;
      uVar4 = uVar4 | 0x8000;
    }
  } while( true );
}



undefined2 __cdecl16near FUN_2388_151c(int *param_1)

{
  undefined2 uVar1;
  int iVar2;
  int *piVar3;
  undefined2 unaff_DS;
  
  piVar3 = (int *)0x4808;
  if ((((param_1 == (int *)0x46ce) || (piVar3 = (int *)0x480a, param_1 == (int *)0x46d6)) ||
      (piVar3 = (int *)0x480c, param_1 == (int *)0x46e6)) &&
     (((*(byte *)(param_1 + 3) & 0xc) == 0 && ((*(byte *)(param_1 + 0x50) & 1) == 0)))) {
    iVar2 = *piVar3;
    if (iVar2 == 0) {
      iVar2 = thunk_FUN_2388_241f(0x200);
      if (iVar2 == 0) goto LAB_2388_1589;
      *piVar3 = iVar2;
    }
    param_1[2] = iVar2;
    *param_1 = iVar2;
    param_1[1] = 0x200;
    param_1[0x51] = 0x200;
    *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 2;
    *(byte *)(param_1 + 0x50) = 0x11;
    uVar1 = 1;
  }
  else {
LAB_2388_1589:
    uVar1 = 0;
  }
  return uVar1;
}



void __cdecl16near FUN_2388_158f(int param_1,undefined2 *param_2)

{
  undefined2 unaff_DS;
  
  if (((*(byte *)(param_2 + 0x50) & 0x10) != 0) &&
     ((*(byte *)(*(byte *)((int)param_2 + 7) + 0x4577) & 0x40) != 0)) {
    FUN_2388_15ce(param_2);
    if (param_1 != 0) {
      *(byte *)(param_2 + 0x50) = 0;
      param_2[0x51] = 0;
      *param_2 = 0;
      param_2[2] = 0;
    }
  }
  return;
}



undefined2 __cdecl16far FUN_2388_15ce(int *param_1)

{
  int iVar1;
  int iVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  
  uVar3 = 0;
  if (param_1 == (int *)0x0) {
    uVar3 = FUN_2388_164a(0);
  }
  else {
    if (((*(byte *)(param_1 + 3) & 3) == 2) &&
       (((*(byte *)(param_1 + 3) & 8) != 0 || ((*(byte *)(param_1 + 0x50) & 1) != 0)))) {
      iVar1 = *param_1 - param_1[2];
      if (0 < iVar1) {
        iVar2 = FUN_2388_1d36(0x2388,*(undefined1 *)((int)param_1 + 7),param_1[2],iVar1);
        if (iVar1 != iVar2) {
          *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 0x20;
          uVar3 = 0xffff;
        }
      }
    }
    *param_1 = param_1[2];
    param_1[1] = 0;
  }
  return uVar3;
}



int FUN_2388_164a(int param_1)

{
  int iVar1;
  uint uVar2;
  int iVar3;
  undefined2 unaff_DS;
  int local_4;
  
  iVar3 = 0;
  local_4 = 0;
  for (uVar2 = 0x46c6; uVar2 <= *(uint *)0x4806; uVar2 = uVar2 + 8) {
    if ((*(byte *)(uVar2 + 6) & 0x83) != 0) {
      iVar1 = FUN_2388_15ce(uVar2);
      if (iVar1 == -1) {
        local_4 = -1;
      }
      else {
        iVar3 = iVar3 + 1;
      }
    }
  }
  if (param_1 == 1) {
    local_4 = iVar3;
  }
  return local_4;
}



undefined2 __cdecl16far FUN_2388_16a6(undefined2 param_1,char *param_2)

{
  char cVar1;
  byte bVar2;
  undefined2 uVar3;
  undefined2 unaff_DS;
  
  FUN_2388_029e();
  cVar1 = *param_2;
  if (cVar1 == '\0') {
    return 0;
  }
  if ((byte)(cVar1 - 0x20U) < 0x59) {
    bVar2 = *(byte *)(ulong)((byte)(cVar1 - 0x20U) + 0x480e) & 0xf;
  }
  else {
    bVar2 = 0;
  }
                    // WARNING: Could not emulate address calculation at 0x00024f6b
                    // WARNING: Treating indirect jump as call
  uVar3 = (*(code *)*(undefined2 *)
                     ((char)(*(byte *)(ulong)((byte)(bVar2 * '\b') + 0x480e) >> 4) * 2 + 0x1696))
                    (cVar1);
  return uVar3;
}



undefined2 * __cdecl16far FUN_2388_1b7e(void)

{
  undefined2 *puVar1;
  undefined2 unaff_DS;
  
  puVar1 = (undefined2 *)0x46c6;
  while( true ) {
    if ((undefined2 *)*(undefined2 *)0x4806 < puVar1) {
      return (undefined2 *)0x0;
    }
    if ((*(byte *)(puVar1 + 3) & 0x83) == 0) break;
    puVar1 = puVar1 + 4;
  }
  puVar1[1] = 0;
  *(undefined1 *)(puVar1 + 3) = 0;
  puVar1[2] = 0;
  *puVar1 = 0;
  *(undefined1 *)((int)puVar1 + 7) = 0xff;
  return puVar1;
}



void FUN_2388_1bb2(undefined2 param_1,uint param_2)

{
  code *pcVar1;
  undefined2 unaff_DS;
  bool bVar2;
  
  bVar2 = param_2 < *(uint *)0x4575;
  if (bVar2) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    if (!bVar2) {
      *(undefined1 *)(param_2 + 0x4577) = 0;
    }
  }
  FUN_2388_1238();
  return;
}



void FUN_2388_1bd2(undefined2 param_1,uint param_2,uint param_3,uint param_4,uint param_5)

{
  uint uVar1;
  code *pcVar2;
  uint uVar3;
  uint uVar4;
  undefined2 unaff_DS;
  bool bVar5;
  undefined4 uVar6;
  
  if (*(uint *)0x4575 <= param_2) goto LAB_2388_1c49;
  bVar5 = false;
  if ((param_4 & 0x8000) != 0) {
    if (param_5 == 0) goto LAB_2388_1c49;
    bVar5 = false;
    pcVar2 = (code *)swi(0x21);
    uVar6 = (*pcVar2)();
    uVar3 = (uint)((ulong)uVar6 >> 0x10);
    if (bVar5) goto LAB_2388_1c49;
    if ((param_5 & 2) == 0) {
      uVar1 = (uint)CARRY2((uint)uVar6,param_3);
      bVar5 = CARRY2(uVar3,param_4) || CARRY2(uVar3 + param_4,uVar1);
      if ((int)(uVar3 + param_4 + uVar1) < 0) goto LAB_2388_1c49;
    }
    else {
      pcVar2 = (code *)swi(0x21);
      uVar6 = (*pcVar2)(uVar3);
      uVar4 = (uint)((ulong)uVar6 >> 0x10);
      uVar3 = (uint)CARRY2((uint)uVar6,param_3);
      uVar1 = uVar4 + param_4;
      bVar5 = CARRY2(uVar4,param_4) || CARRY2(uVar1,uVar3);
      if ((int)(uVar1 + uVar3) < 0) {
        pcVar2 = (code *)swi(0x21);
        (*pcVar2)();
        goto LAB_2388_1c49;
      }
    }
  }
  pcVar2 = (code *)swi(0x21);
  (*pcVar2)();
  if (!bVar5) {
    *(byte *)(param_2 + 0x4577) = *(byte *)(param_2 + 0x4577) & 0xfd;
  }
LAB_2388_1c49:
  FUN_2388_124d();
  return;
}



// WARNING: Removing unreachable block (ram,0x000255b3)
// WARNING: Removing unreachable block (ram,0x000255a8)

void FUN_2388_1c4c(undefined2 param_1,uint param_2,undefined2 param_3,int param_4)

{
  char *pcVar1;
  char cVar2;
  code *pcVar3;
  undefined2 uVar4;
  undefined1 extraout_AH;
  undefined1 extraout_AH_00;
  int iVar5;
  undefined1 extraout_AH_01;
  int iVar7;
  uint extraout_DX;
  char *pcVar8;
  char *pcVar9;
  undefined2 unaff_DS;
  undefined1 uVar10;
  bool bVar11;
  undefined4 uVar12;
  char cVar6;
  
  if (((*(uint *)0x4575 <= param_2) || (param_4 == 0)) || ((*(byte *)(param_2 + 0x4577) & 2) != 0))
  {
LAB_2388_1cc9:
    FUN_2388_124d();
    return;
  }
  uVar10 = *(uint *)0x4896 < 0xd6d6;
  if (*(uint *)0x4896 == 0xd6d6) {
    (*(code *)*(undefined2 *)0x4898)();
  }
  pcVar3 = (code *)swi(0x21);
  uVar12 = (*pcVar3)();
  pcVar9 = (char *)((ulong)uVar12 >> 0x10);
  if ((((bool)uVar10) || ((*(byte *)(param_2 + 0x4577) & 0x80) == 0)) ||
     (*(byte *)(param_2 + 0x4577) = *(byte *)(param_2 + 0x4577) & 0xfb, (int)uVar12 == 0))
  goto LAB_2388_1cc9;
  uVar4 = 0xd00;
  if (*pcVar9 == '\n') {
    *(byte *)(param_2 + 0x4577) = *(byte *)(param_2 + 0x4577) | 4;
  }
LAB_2388_1cae:
  pcVar8 = (char *)((ulong)uVar12 >> 0x10);
  iVar7 = (int)uVar12;
  pcVar1 = pcVar8 + 1;
  cVar2 = *pcVar8;
  cVar6 = (char)((uint)uVar4 >> 8);
  uVar4 = CONCAT11(cVar6,cVar2);
  if (cVar2 == cVar6) {
    if (iVar7 != 1) {
      if (*pcVar1 != '\n') goto LAB_2388_1cbe;
      goto LAB_2388_1cc1;
    }
    bVar11 = false;
    if ((*(byte *)(param_2 + 0x4577) & 0x40) == 0) {
      pcVar3 = (code *)swi(0x21);
      iVar5 = (*pcVar3)();
      if (!bVar11) {
        uVar10 = 0;
        if (iVar5 != 0) {
          pcVar3 = (code *)swi(0x21);
          (*pcVar3)();
          iVar7 = 1;
          uVar10 = extraout_AH_01;
        }
        uVar4 = CONCAT11(uVar10,0xd);
        goto LAB_2388_1cbe;
      }
      goto LAB_2388_1cc9;
    }
    pcVar3 = (code *)swi(0x21);
    (*pcVar3)();
    bVar11 = false;
    uVar10 = extraout_AH;
    if ((extraout_DX & 0x20) == 0) {
      pcVar3 = (code *)swi(0x21);
      (*pcVar3)();
      uVar10 = extraout_AH_00;
      if (bVar11) goto LAB_2388_1cc9;
    }
    uVar4 = CONCAT11(uVar10,10);
  }
  else if (cVar2 == '\x1a') {
    *(byte *)(param_2 + 0x4577) = *(byte *)(param_2 + 0x4577) | 2;
    goto LAB_2388_1cc9;
  }
LAB_2388_1cbe:
  *pcVar9 = (char)uVar4;
  pcVar9 = pcVar9 + 1;
LAB_2388_1cc1:
  uVar12 = CONCAT22(pcVar1,iVar7 + -1);
  if (iVar7 + -1 == 0) goto LAB_2388_1cc9;
  goto LAB_2388_1cae;
}



// WARNING: Unable to track spacebase fully for stack

undefined2 FUN_2388_1d36(undefined2 param_1,uint param_2,char *param_3,int param_4)

{
  char *pcVar1;
  code *pcVar2;
  char cVar3;
  undefined2 uVar4;
  uint uVar5;
  int iVar6;
  char *pcVar7;
  char *pcVar8;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar9;
  
  if (*(uint *)0x4575 <= param_2) {
LAB_2388_1d49:
    uVar4 = FUN_2388_124d();
    return uVar4;
  }
  if (*(int *)0x4896 == -0x292a) {
    (*(code *)*(undefined2 *)0x4898)();
  }
  if ((*(byte *)(param_2 + 0x4577) & 0x20) != 0) {
    bVar9 = false;
    pcVar2 = (code *)swi(0x21);
    (*pcVar2)();
    if (bVar9) goto LAB_2388_1d49;
  }
  if ((*(byte *)(param_2 + 0x4577) & 0x80) != 0) {
    bVar9 = true;
    iVar6 = param_4;
    pcVar8 = param_3;
    if (param_4 != 0) {
      do {
        if (iVar6 == 0) break;
        iVar6 = iVar6 + -1;
        pcVar1 = pcVar8;
        pcVar8 = pcVar8 + 1;
        bVar9 = *pcVar1 == '\n';
      } while (!bVar9);
      if (!bVar9) goto LAB_2388_1de1;
      pcVar7 = param_3;
      uVar5 = FUN_2388_23de();
      if (uVar5 < 0xa9) {
        uVar4 = FUN_2388_029e();
        bVar9 = pcVar8 < pcVar7;
        if (pcVar8 != pcVar7) {
          pcVar2 = (code *)swi(0x21);
          uVar5 = (*pcVar2)(iVar6,param_2);
          if ((bVar9) || (uVar5 < (uint)((int)pcVar8 - (int)pcVar7))) {
            uVar4 = FUN_2388_124d();
            return uVar4;
          }
        }
        return uVar4;
      }
      pcVar7 = &stack0xfff0;
      pcVar8 = &stack0xfff2;
      do {
        pcVar1 = param_3;
        param_3 = param_3 + 1;
        cVar3 = *pcVar1;
        if (cVar3 == '\n') {
          cVar3 = '\r';
          if (pcVar8 == pcVar7) {
            cVar3 = FUN_2388_1dea();
          }
          pcVar1 = pcVar8;
          pcVar8 = pcVar8 + 1;
          *pcVar1 = cVar3;
          cVar3 = '\n';
        }
        if (pcVar8 == pcVar7) {
          cVar3 = FUN_2388_1dea();
        }
        pcVar1 = pcVar8;
        pcVar8 = pcVar8 + 1;
        *pcVar1 = cVar3;
        param_4 = param_4 + -1;
      } while (param_4 != 0);
      FUN_2388_1dea();
    }
    uVar4 = FUN_2388_1e34();
    return uVar4;
  }
LAB_2388_1de1:
  uVar4 = FUN_2388_1e42();
  return uVar4;
}



// WARNING: Unable to track spacebase fully for stack

undefined2 __cdecl16near FUN_2388_1dea(void)

{
  code *pcVar1;
  undefined2 in_AX;
  uint uVar2;
  undefined2 uVar3;
  uint in_DX;
  int unaff_BP;
  uint unaff_DI;
  undefined2 unaff_SS;
  bool bVar4;
  
  bVar4 = unaff_DI < in_DX;
  if (unaff_DI != in_DX) {
    pcVar1 = (code *)swi(0x21);
    uVar2 = (*pcVar1)();
    if ((bVar4) ||
       (*(int *)(unaff_BP + -2) = *(int *)(unaff_BP + -2) + uVar2, uVar2 < unaff_DI - in_DX)) {
      uVar3 = FUN_2388_124d();
      return uVar3;
    }
  }
  return in_AX;
}



// WARNING: Stack frame is not setup normally: Input value of stackpointer is not used

void FUN_2388_1e34(void)

{
  FUN_2388_124d();
  return;
}



void FUN_2388_1e42(void)

{
  code *pcVar1;
  int unaff_BP;
  undefined2 unaff_SS;
  
  if (*(int *)(unaff_BP + 10) != 0) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    FUN_2388_124d();
    return;
  }
  FUN_2388_124d();
  return;
}



int __cdecl16far FUN_2388_1e76(byte *param_1)

{
  byte *pbVar1;
  byte bVar2;
  byte bVar3;
  int iVar4;
  undefined2 unaff_DS;
  
  iVar4 = 0;
  do {
    do {
      pbVar1 = param_1;
      param_1 = param_1 + 1;
      bVar2 = *pbVar1;
    } while (bVar2 == 0x20);
  } while (bVar2 == 9);
  if ((bVar2 != 0x2d) && (bVar3 = bVar2, bVar2 != 0x2b)) goto LAB_2388_1e96;
  while( true ) {
    pbVar1 = param_1;
    param_1 = param_1 + 1;
    bVar3 = *pbVar1;
LAB_2388_1e96:
    if ((0x39 < bVar3) || (bVar3 < 0x30)) break;
    iVar4 = iVar4 * 10 + (uint)(byte)(bVar3 - 0x30);
  }
  if (bVar2 == 0x2d) {
    iVar4 = -iVar4;
  }
  return iVar4;
}



undefined4 __stdcall16far FUN_2388_1eca(uint param_1,uint param_2,uint param_3,uint param_4)

{
  uint uVar1;
  uint uVar2;
  
  uVar1 = param_2 - param_4;
  uVar2 = uVar1 * 0x10 + param_1;
  return CONCAT22((((((uint)(param_2 < param_4) * -2 + (uint)CARRY2(uVar1,uVar1)) * 2 +
                    (uint)CARRY2(uVar1 * 2,uVar1 * 2)) * 2 + (uint)CARRY2(uVar1 * 4,uVar1 * 4)) * 2
                   + (uint)CARRY2(uVar1 * 8,uVar1 * 8) + (uint)CARRY2(uVar1 * 0x10,param_1)) -
                  (uint)(uVar2 < param_3),uVar2 - param_3);
}



int __cdecl16far FUN_2388_1ef6(int *param_1)

{
  uint uVar1;
  char *pcVar2;
  char *pcVar3;
  undefined2 unaff_DS;
  long lVar4;
  long lVar5;
  int local_e;
  int local_a;
  int local_6;
  
  uVar1 = (uint)*(byte *)((int)param_1 + 7);
  if (param_1[1] < 0) {
    param_1[1] = 0;
  }
  lVar4 = FUN_2388_1bd2(0x2388,uVar1,0,0,1);
  local_6 = (int)lVar4;
  if (lVar4 < 0) {
LAB_2388_1f3c:
    local_a = -1;
  }
  else {
    if (((*(byte *)(param_1 + 3) & 8) == 0) && ((*(byte *)(param_1 + 0x50) & 1) == 0)) {
      return local_6 - param_1[1];
    }
    local_a = *param_1 - param_1[2];
    if ((*(byte *)(param_1 + 3) & 3) == 0) {
      if ((*(byte *)(param_1 + 3) & 0x80) == 0) {
        *(undefined2 *)0x4568 = 0x16;
        goto LAB_2388_1f3c;
      }
    }
    else if ((*(byte *)(uVar1 + 0x4577) & 0x80) != 0) {
      for (pcVar3 = (char *)param_1[2]; pcVar3 < (char *)*param_1; pcVar3 = pcVar3 + 1) {
        if (*pcVar3 == '\n') {
          local_a = local_a + 1;
        }
      }
    }
    if (lVar4 != 0) {
      if ((*(byte *)(param_1 + 3) & 1) != 0) {
        if (param_1[1] == 0) {
          local_a = 0;
        }
        else {
          local_e = (*param_1 - param_1[2]) + param_1[1];
          if ((*(byte *)(uVar1 + 0x4577) & 0x80) != 0) {
            lVar5 = FUN_2388_1bd2(0x2388,uVar1,0,0,2);
            if (lVar5 == lVar4) {
              pcVar2 = (char *)(local_e + param_1[2]);
              for (pcVar3 = (char *)param_1[2]; pcVar3 < pcVar2; pcVar3 = pcVar3 + 1) {
                if (*pcVar3 == '\n') {
                  local_e = local_e + 1;
                }
              }
              if ((*(byte *)(param_1 + 0x50) & 0x20) != 0) {
                local_e = local_e + 1;
              }
            }
            else {
              FUN_2388_1bd2(0x2388,uVar1,lVar4,0);
              local_e = param_1[0x51];
              if ((*(byte *)(uVar1 + 0x4577) & 4) != 0) {
                local_e = local_e + 1;
              }
            }
          }
          local_6 = local_6 - local_e;
        }
      }
      local_a = local_6 + local_a;
    }
  }
  return local_a;
}



undefined2 __cdecl16far FUN_2388_206c(int *param_1,int param_2,uint param_3,uint param_4)

{
  int *piVar1;
  undefined2 unaff_DS;
  undefined2 local_4;
  
  local_4 = 0;
  if ((param_3 == 4) ||
     (((param_4 != 0 && (param_4 < 0x8000)) && ((param_3 == 0 || (param_3 == 0x40)))))) {
    piVar1 = param_1 + 0x50;
    FUN_2388_15ce(param_1);
    FUN_2388_1408(param_1);
    if ((param_3 & 4) == 0) {
      if (param_2 == 0) {
        param_2 = thunk_FUN_2388_241f(param_4);
        if (param_2 == 0) {
          return 0xffff;
        }
        *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) & 0xfb;
        *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 8;
        *(undefined1 *)piVar1 = 0;
      }
      else {
        *(int *)0x4870 = *(int *)0x4870 + 1;
        *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) & 0xf3;
        *(undefined1 *)piVar1 = 1;
      }
    }
    else {
      *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 4;
      *(undefined1 *)piVar1 = 0;
      param_2 = (int)param_1 + 0xa1;
      param_4 = 1;
    }
    param_1[0x51] = param_4;
    param_1[2] = param_2;
    *param_1 = param_2;
    param_1[1] = 0;
  }
  else {
    local_4 = 0xffff;
  }
  return local_4;
}



void __cdecl16far FUN_2388_212c(undefined2 *param_1,undefined2 *param_2,uint param_3)

{
  undefined2 *puVar1;
  undefined2 *puVar2;
  uint uVar3;
  undefined2 unaff_DS;
  
  if (param_3 != 0) {
    if (((uint)param_1 & 1) != 0) {
      puVar2 = param_1;
      param_1 = (undefined2 *)((int)param_1 + 1);
      puVar1 = param_2;
      param_2 = (undefined2 *)((int)param_2 + 1);
      *(undefined1 *)puVar2 = *(undefined1 *)puVar1;
      param_3 = param_3 - 1;
    }
    for (uVar3 = param_3 >> 1; uVar3 != 0; uVar3 = uVar3 - 1) {
      puVar2 = param_1;
      param_1 = param_1 + 1;
      puVar1 = param_2;
      param_2 = param_2 + 1;
      *puVar2 = *puVar1;
    }
    for (uVar3 = (uint)((param_3 & 1) != 0); uVar3 != 0; uVar3 = uVar3 - 1) {
      puVar2 = param_1;
      param_1 = (undefined2 *)((int)param_1 + 1);
      puVar1 = param_2;
      param_2 = (undefined2 *)((int)param_2 + 1);
      *(undefined1 *)puVar2 = *(undefined1 *)puVar1;
    }
  }
  return;
}



byte * __cdecl16far FUN_2388_2158(void)

{
  uint uVar1;
  byte *pbVar2;
  ulong uVar3;
  byte bVar4;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  char in_BL;
  int iVar9;
  int unaff_BP;
  byte *pbVar10;
  byte *pbVar11;
  byte *pbVar12;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar13;
  char cVar5;
  
  uVar1 = *(uint *)(unaff_BP + 0xc);
  iVar9 = *(int *)(unaff_BP + 6);
  uVar7 = *(uint *)(unaff_BP + 8);
  pbVar2 = (byte *)*(undefined2 *)(unaff_BP + 10);
  pbVar11 = pbVar2;
  pbVar10 = pbVar2;
  if (((in_BL != '\0') && (uVar1 == 10)) && ((int)uVar7 < 0)) {
    pbVar11 = pbVar2 + 1;
    *pbVar2 = 0x2d;
    bVar13 = iVar9 != 0;
    iVar9 = -iVar9;
    uVar7 = -(uVar7 + bVar13);
    pbVar10 = pbVar11;
  }
  do {
    uVar8 = 0;
    uVar6 = uVar7;
    if (uVar7 != 0) {
      uVar6 = uVar7 / uVar1;
      uVar8 = uVar7 % uVar1;
    }
    uVar3 = CONCAT22(uVar8,iVar9);
    iVar9 = (int)(uVar3 / uVar1);
    cVar5 = (char)(uVar3 % (ulong)uVar1);
    bVar4 = cVar5 + 0x30;
    if (0x39 < bVar4) {
      bVar4 = cVar5 + 0x57;
    }
    pbVar12 = pbVar11 + 1;
    *pbVar11 = bVar4;
    uVar7 = uVar6;
    pbVar11 = pbVar12;
  } while (uVar6 != 0 || iVar9 != 0);
  *pbVar12 = 0;
  do {
    pbVar12 = pbVar12 + -1;
    LOCK();
    bVar4 = *pbVar12;
    *pbVar12 = *pbVar10;
    UNLOCK();
    *pbVar10 = bVar4;
    pbVar11 = pbVar10 + 2;
    pbVar10 = pbVar10 + 1;
  } while (pbVar11 < pbVar12);
  return pbVar2;
}



void __cdecl16near FUN_2388_21b8(void)

{
  undefined2 uVar1;
  int iVar2;
  undefined2 unaff_DS;
  
  LOCK();
  uVar1 = *(undefined2 *)0x4892;
  *(undefined2 *)0x4892 = 0x400;
  UNLOCK();
  iVar2 = thunk_FUN_2388_241f();
  *(undefined2 *)0x4892 = uVar1;
  if (iVar2 != 0) {
    return;
  }
  FUN_2388_00f1();
  return;
}



void __cdecl16near FUN_2388_21de(int *param_1)

{
  int iVar1;
  undefined2 unaff_DS;
  
  iVar1 = thunk_FUN_2388_241f(0x200);
  if (iVar1 == 0) {
    *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 4;
    param_1[0x51] = 1;
    iVar1 = (int)param_1 + 0xa1;
  }
  else {
    *(byte *)(param_1 + 3) = *(byte *)(param_1 + 3) | 8;
    param_1[0x51] = 0x200;
  }
  *param_1 = iVar1;
  param_1[2] = iVar1;
  param_1[1] = 0;
  return;
}



// WARNING: Removing unreachable block (ram,0x00025bb2)

uint __cdecl16far FUN_2388_2222(undefined2 param_1,uint param_2,char param_3,uint param_4)

{
  code *pcVar1;
  char cVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  byte bVar6;
  uint extraout_DX;
  undefined2 unaff_DS;
  bool bVar7;
  undefined1 uVar8;
  bool bVar9;
  byte local_6;
  char local_4;
  char local_3;
  
  cVar2 = '\0';
  if (2 < *(byte *)0x4570) {
    cVar2 = param_3;
  }
  _param_3 = param_4;
  local_6 = 0;
  if (((param_2 & 0x8000) == 0) && (((param_2 & 0x4000) != 0 || ((*(byte *)0x4891 & 0x80) == 0)))) {
    local_6 = 0x80;
  }
  bVar7 = false;
  pcVar1 = (code *)swi(0x21);
  uVar5 = param_2;
  uVar3 = (*pcVar1)();
  if (bVar7) {
    if ((uVar3 != 2) || ((uVar5 & 0x100) == 0)) goto LAB_2388_2280;
    bVar7 = false;
    local_4 = -0x57;
    FUN_2388_23cd();
    uVar8 = 0;
    uVar5 = 0;
    _param_3 = param_4;
LAB_2388_233c:
    pcVar1 = (code *)swi(0x21);
    uVar3 = (*pcVar1)();
    if ((bool)uVar8) {
LAB_2388_2345:
      uVar5 = FUN_2388_124d();
      return uVar5;
    }
    if ((local_4 != '\0') || ((param_2 & 2) == 0)) {
      pcVar1 = (code *)swi(0x21);
      (*pcVar1)();
      bVar9 = false;
      pcVar1 = (code *)swi(0x21);
      uVar3 = (*pcVar1)();
      if (bVar9) goto LAB_2388_2345;
      if ((!bVar7) && ((_param_3 & 1) != 0)) {
        bVar7 = false;
        uVar5 = (uint)(byte)((byte)uVar5 | 1);
        pcVar1 = (code *)swi(0x21);
        (*pcVar1)();
        if (bVar7) goto LAB_2388_2345;
      }
    }
  }
  else {
    if ((uVar5 & 0x500) == 0x500) {
      pcVar1 = (code *)swi(0x21);
      (*pcVar1)();
      goto LAB_2388_2280;
    }
    bVar7 = true;
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    if ((extraout_DX & 0x80) != 0) {
      local_6 = local_6 | 0x40;
    }
    if ((local_6 & 0x40) == 0) {
      if ((param_2 & 0x200) == 0) {
        if (((local_6 & 0x80) != 0) && ((param_2 & 2) != 0)) {
          pcVar1 = (code *)swi(0x21);
          (*pcVar1)();
          pcVar1 = (code *)swi(0x21);
          iVar4 = (*pcVar1)();
          if ((iVar4 != 0) && (local_3 == '\x1a')) {
            pcVar1 = (code *)swi(0x21);
            (*pcVar1)();
            pcVar1 = (code *)swi(0x21);
            (*pcVar1)();
          }
          uVar5 = 0;
          pcVar1 = (code *)swi(0x21);
          (*pcVar1)();
        }
      }
      else {
        uVar8 = 0;
        if ((param_2 & 3) == 0) {
          pcVar1 = (code *)swi(0x21);
          (*pcVar1)();
          pcVar1 = (code *)swi(0x21);
          (*pcVar1)();
          local_4 = cVar2;
          goto LAB_2388_233c;
        }
        uVar5 = 0;
        pcVar1 = (code *)swi(0x21);
        (*pcVar1)();
      }
    }
  }
  if ((local_6 & 0x40) == 0) {
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    bVar6 = 0;
    if ((uVar5 & 1) != 0) {
      bVar6 = 0x10;
    }
    if ((param_2 & 8) != 0) {
      bVar6 = bVar6 | 0x20;
    }
  }
  else {
    bVar6 = 0;
  }
  if (uVar3 < *(uint *)0x4575) {
    *(byte *)(uVar3 + 0x4577) = bVar6 | local_6 | 1;
    return uVar3;
  }
  pcVar1 = (code *)swi(0x21);
  (*pcVar1)();
LAB_2388_2280:
  uVar5 = FUN_2388_124d();
  return uVar5;
}



void __cdecl16near FUN_2388_23cd(void)

{
  return;
}



int __cdecl16far FUN_2388_23de(void)

{
  int iVar1;
  undefined2 unaff_DS;
  
  if ((undefined1 *)*(uint *)0x45a2 < &stack0x0004) {
    iVar1 = -((int)*(uint *)0x45a2 - (int)&stack0x0004);
  }
  else {
    iVar1 = 0;
  }
  return iVar1;
}



undefined2 __cdecl16far thunk_FUN_2388_241f(uint param_1)

{
  undefined2 uVar1;
  bool bVar2;
  
  bVar2 = param_1 < 0xffe8;
  if (param_1 < 0xffe9) {
    uVar1 = FUN_2388_2546();
    if (!bVar2) {
      return uVar1;
    }
    FUN_2388_2448();
    if ((!bVar2) && (uVar1 = FUN_2388_2546(), !bVar2)) {
      return uVar1;
    }
  }
  return 0;
}



void __cdecl16far thunk_FUN_2388_23fe(uint param_1)

{
  byte *pbVar1;
  undefined2 unaff_DS;
  
  if (*(uint *)0x453a < param_1) {
    pbVar1 = (byte *)(param_1 - 2);
    *pbVar1 = *pbVar1 | 1;
    if (pbVar1 < (byte *)*(undefined2 *)0x453c) {
      *(undefined2 *)0x453c = pbVar1;
    }
  }
  return;
}



void __cdecl16far FUN_2388_23fe(uint param_1)

{
  byte *pbVar1;
  undefined2 unaff_DS;
  
  if (*(uint *)0x453a < param_1) {
    pbVar1 = (byte *)(param_1 - 2);
    *pbVar1 = *pbVar1 | 1;
    if (pbVar1 < (byte *)*(undefined2 *)0x453c) {
      *(undefined2 *)0x453c = pbVar1;
    }
  }
  return;
}



undefined2 __cdecl16far FUN_2388_241f(uint param_1)

{
  undefined2 uVar1;
  bool bVar2;
  
  bVar2 = param_1 < 0xffe8;
  if (param_1 < 0xffe9) {
    uVar1 = FUN_2388_2546();
    if (!bVar2) {
      return uVar1;
    }
    FUN_2388_2448();
    if ((!bVar2) && (uVar1 = FUN_2388_2546(), !bVar2)) {
      return uVar1;
    }
  }
  return 0;
}



void __cdecl16near FUN_2388_2448(void)

{
  int *piVar1;
  int iVar2;
  uint uVar3;
  int in_CX;
  uint uVar4;
  int iVar5;
  int in_BX;
  uint *unaff_SI;
  undefined2 *puVar6;
  undefined2 unaff_SS;
  undefined2 unaff_DS;
  bool bVar7;
  
  if ((*(byte *)(in_BX + 2) & 1) != 0) {
    FUN_2388_2525();
    if ((*unaff_SI & 1) != 0) {
      in_CX = (in_CX - *unaff_SI) + -1;
    }
    uVar3 = *(uint *)(in_BX + 4);
    if (uVar3 != 0) {
      if (!CARRY2(in_CX + 2U,uVar3)) {
        uVar3 = *(uint *)0x4892;
        if (uVar3 == 0x2000) goto LAB_2388_2495;
        uVar4 = 0x8000;
        while (uVar3 <= uVar4) {
          uVar4 = uVar4 >> 1;
          if (uVar4 == 0) goto LAB_2388_24ae;
        }
        if (uVar4 < 8) goto LAB_2388_24ae;
        uVar3 = uVar4 << 1;
        goto LAB_2388_2495;
      }
      uVar4 = 0xfff0;
      if (in_CX + 2U + uVar3 == 0) {
        while( true ) {
          bVar7 = false;
          iVar2 = FUN_2388_24d4();
          if (!bVar7) break;
          if (uVar4 == 0xfff0) {
            return;
          }
LAB_2388_24ae:
          uVar3 = 0x10;
LAB_2388_2495:
          uVar4 = ~(uVar3 - 1);
        }
        iVar5 = iVar2 - *(int *)(in_BX + 4);
        *(int *)(in_BX + 4) = iVar2;
        *(undefined2 *)(in_BX + 8) = unaff_SI;
        piVar1 = (int *)*(int *)(in_BX + 10);
        *piVar1 = iVar5 + -1;
        puVar6 = (undefined2 *)((int)piVar1 + iVar5);
        *puVar6 = 0xfffe;
        *(undefined2 *)(in_BX + 10) = puVar6;
      }
    }
  }
  return;
}



void __cdecl16near FUN_2388_24d4(int param_1)

{
  code *pcVar1;
  uint in_AX;
  uint uVar2;
  int extraout_DX;
  int in_BX;
  int unaff_DS;
  bool bVar3;
  
  if ((((*(byte *)(in_BX + 2) & 4) == 0) || (in_AX - 1 < *(int *)(in_BX + 4) - 1U)) ||
     (*(uint *)(in_BX + -2) < in_AX - 1)) {
    uVar2 = in_AX >> 4;
    if (uVar2 == 0) {
      uVar2 = 0x1000;
    }
    bVar3 = false;
    if ((*(byte *)(in_BX + 2) & 4) != 0) {
      bVar3 = uVar2 + unaff_DS < *(uint *)0x456e;
    }
    pcVar1 = (code *)swi(0x21);
    (*pcVar1)();
    if ((!bVar3) && ((*(byte *)(param_1 + 2) & 4) != 0)) {
      *(int *)(param_1 + -2) = extraout_DX + -1;
    }
  }
  return;
}



void __cdecl16near FUN_2388_2525(void)

{
  int in_BX;
  uint *puVar1;
  undefined2 unaff_DS;
  
  puVar1 = (uint *)*(undefined2 *)(in_BX + 8);
  if (puVar1 == (uint *)*(undefined2 *)(in_BX + 10)) {
    puVar1 = (uint *)*(undefined2 *)(in_BX + 6);
  }
  while( true ) {
    if (*puVar1 == 0xfffe) break;
    puVar1 = (uint *)((int)puVar1 + (*puVar1 & 0xfffe) + 2);
  }
  return;
}



uint * __cdecl16near FUN_2388_2546(void)

{
  uint *puVar1;
  uint uVar2;
  uint uVar3;
  int in_CX;
  uint uVar4;
  int in_BX;
  uint *puVar5;
  uint *puVar6;
  uint *puVar7;
  undefined2 unaff_DS;
  
  uVar4 = in_CX + 1U & 0xfffe;
  puVar7 = (uint *)*(undefined2 *)(in_BX + 8);
  puVar5 = (uint *)*(undefined2 *)(in_BX + 10);
  do {
    while( true ) {
      puVar1 = puVar7 + 1;
      uVar3 = *puVar7;
      puVar6 = puVar1;
      if ((uVar3 & 1) != 0) {
        while( true ) {
          uVar2 = uVar3 - 1;
          if (uVar4 <= uVar2) {
            *puVar7 = uVar4;
            puVar7 = puVar1;
            if (uVar2 != uVar4) {
              *(int *)((int)puVar1 + uVar4) = (uVar2 - uVar4) + -1;
              puVar7 = (uint *)((int)((int)puVar1 + uVar4) - uVar4);
            }
            *(int *)(in_BX + 8) = (int)puVar7 + uVar4;
            return puVar1;
          }
          if (CARRY2((uint)puVar1,uVar2)) goto LAB_2388_259f;
          puVar6 = (uint *)((int)puVar1 + uVar2) + 1;
          uVar3 = *(uint *)((int)puVar1 + uVar2);
          if ((uVar3 & 1) == 0) break;
          uVar3 = uVar3 + uVar2 + 2;
          *puVar7 = uVar3;
        }
      }
      if (puVar6 + -1 < puVar5) break;
      if (((uint)puVar5 & 1) != 0) goto LAB_2388_259f;
      puVar7 = (uint *)*(undefined2 *)(in_BX + 6);
      if ((uint *)*(undefined2 *)(in_BX + 8) == puVar7) goto LAB_2388_259f;
      puVar5 = (uint *)((int)*(undefined2 *)(in_BX + 8) + -1);
    }
    puVar7 = (uint *)((int)puVar6 + uVar3);
  } while (!CARRY2((uint)puVar6,uVar3));
LAB_2388_259f:
  puVar7 = (uint *)*(undefined2 *)(in_BX + 6);
  *(undefined2 *)(in_BX + 8) = puVar7;
  return puVar7;
}


