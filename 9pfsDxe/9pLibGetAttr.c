/** @file
  9P library.

Copyright (c) 2020, Akira Moroo. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "9pLib.h"

#define BLK_UNIT    512

#define	S_ISDIR(m)	((m & 0170000) == 0040000)	/* directory */
#define	S_ISCHR(m)	((m & 0170000) == 0020000)	/* char special */
#define	S_ISBLK(m)	((m & 0170000) == 0060000)	/* block special */
#define	S_ISREG(m)	((m & 0170000) == 0100000)	/* regular file */
#define	S_ISFIFO(m)	((m & 0170000) == 0010000)	/* fifo */
#define	S_ISLNK(m)	((m & 0170000) == 0120000)	/* symbolic link */
#define	S_ISSOCK(m)	((m & 0170000) == 0140000)	/* socket */

/**
  Converts Epoch seconds (elapsed since 1970 JANUARY 01, 00:00:00 UTC) to EFI_TIME
 **/
STATIC
VOID
EpochToEfiTime (
  IN  UINTN     EpochSeconds,
  OUT EFI_TIME  *Time
  )
{
  UINTN         a;
  UINTN         b;
  UINTN         c;
  UINTN         d;
  UINTN         g;
  UINTN         j;
  UINTN         m;
  UINTN         y;
  UINTN         da;
  UINTN         db;
  UINTN         dc;
  UINTN         dg;
  UINTN         hh;
  UINTN         mm;
  UINTN         ss;
  UINTN         J;

  J  = (EpochSeconds / 86400) + 2440588;
  j  = J + 32044;
  g  = j / 146097;
  dg = j % 146097;
  c  = (((dg / 36524) + 1) * 3) / 4;
  dc = dg - (c * 36524);
  b  = dc / 1461;
  db = dc % 1461;
  a  = (((db / 365) + 1) * 3) / 4;
  da = db - (a * 365);
  y  = (g * 400) + (c * 100) + (b * 4) + a;
  m  = (((da * 5) + 308) / 153) - 2;
  d  = da - (((m + 4) * 153) / 5) + 122;

  Time->Year  = y - 4800 + ((m + 2) / 12);
  Time->Month = ((m + 2) % 12) + 1;
  Time->Day   = d + 1;

  ss = EpochSeconds % 60;
  a  = (EpochSeconds - ss) / 60;
  mm = a % 60;
  b = (a - mm) / 60;
  hh = b % 24;

  Time->Hour        = hh;
  Time->Minute      = mm;
  Time->Second      = ss;
  Time->Nanosecond  = 0;

}

EFI_STATUS
P9GetAttr (
  IN P9_VOLUME          *Volume,
  IN OUT P9_IFILE       *IFile
  )
{
  EFI_STATUS                    Status;
  P9TGetAttr                    *TxGetAttr;
  P9RGetAttr                    *RxGetAttr;
  UINTN                         Size;
  EFI_FILE_INFO                 *FileInfo;

  TxGetAttr = AllocateZeroPool (sizeof (P9TGetAttr));
  if (TxGetAttr == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  TxGetAttr->Header.Size  = sizeof (P9TGetAttr);
  TxGetAttr->Header.Id    = Tgetattr;
  TxGetAttr->Header.Tag   = Volume->Tag;
  TxGetAttr->Fid          = IFile->Fid;
  TxGetAttr->RequestMask  = P9_GETATTR_ALL;

  RxGetAttr = AllocateZeroPool (sizeof (P9RGetAttr));
  if (RxGetAttr == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = DoP9 (
    Volume,
    TxGetAttr,
    sizeof (P9TGetAttr),
    RxGetAttr,
    sizeof (P9RGetAttr)
    );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  if (RxGetAttr->Header.Id != Rgetattr) {
    Status = P9Error (RxGetAttr, sizeof (P9RGetAttr));
    goto Exit;
  }

  Size = SIZE_OF_EFI_FILE_INFO;
  Size += StrSize (IFile->FileName);

  if (IFile->FileInfo == NULL) {
    IFile->FileInfo = AllocateZeroPool (Size);
    if (IFile->FileInfo == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
  }

  FileInfo = IFile->FileInfo;

  CopyMem (&IFile->Qid, &RxGetAttr->Qid, QID_SIZE);
  EpochToEfiTime (RxGetAttr->CTimeSec, &FileInfo->CreateTime);
  EpochToEfiTime (RxGetAttr->MTimeSec, &FileInfo->ModificationTime);
  EpochToEfiTime (RxGetAttr->ATimeSec, &FileInfo->LastAccessTime);
  FileInfo->CreateTime.Nanosecond        = (UINT32)RxGetAttr->CTimeNSec;
  FileInfo->ModificationTime.Nanosecond  = (UINT32)RxGetAttr->MTimeNSec;
  FileInfo->LastAccessTime.Nanosecond    = (UINT32)RxGetAttr->ATimeNSec;
  FileInfo->Size                         = Size;
  FileInfo->FileSize                     = RxGetAttr->Size;
  FileInfo->PhysicalSize                 = BLK_UNIT * RxGetAttr->Blocks;
  FileInfo->Attribute                    = S_ISDIR (RxGetAttr->Mode) ? EFI_FILE_DIRECTORY : EFI_FILE_ARCHIVE;
  StrCpyS (FileInfo->FileName, StrLen (IFile->FileName) + 1, IFile->FileName);

  Status = EFI_SUCCESS;

Exit:
  if (TxGetAttr != NULL) {
    FreePool (TxGetAttr);
  }

  if (RxGetAttr != NULL) {
    FreePool (RxGetAttr);
  }

  return Status;
}
