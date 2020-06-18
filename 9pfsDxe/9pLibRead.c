/** @file
  9P library.

Copyright (c) 2020, Akira Moroo. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "9pLib.h"

VOID
EFIAPI
TxReadCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  P9_MESSAGE_PRIVATE_DATA  *Read;

  Read = (P9_MESSAGE_PRIVATE_DATA *)Context;
  Read->IsTxDone = TRUE;
  gBS->CloseEvent (Read->TxIoToken.CompletionToken.Event);
}

VOID
EFIAPI
RxReadCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  P9_MESSAGE_PRIVATE_DATA  *Read;

  Read = (P9_MESSAGE_PRIVATE_DATA *)Context;
  Read->IsRxDone = TRUE;
}

EFI_STATUS
P9LRead (
  IN P9_VOLUME          *Volume,
  IN OUT P9_IFILE       *IFile,
  IN OUT UINT32         *Count,
  OUT VOID              *Data
  )
{
  EFI_STATUS                    Status;
  P9_MESSAGE_PRIVATE_DATA       *Read;
  EFI_TCP4_PROTOCOL             *Tcp4;
  P9TRead                       *TxRead;
  P9RRead                       *RxRead;
  UINTN                         RxReadSize;
  VOID                          *CurrentRxRead;
  INTN                          CurrentRxReadSize;
  UINT32                        CurrentDataLength;
  BOOLEAN                       HasMoreData;

  Tcp4 = Volume->Tcp4;

  Read = AllocateZeroPool (sizeof (P9_MESSAGE_PRIVATE_DATA));
  if (Read == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = gBS->CreateEvent(
      EVT_NOTIFY_SIGNAL,
      TPL_CALLBACK,
      TxReadCallback,
      Read,
      &Read->TxIoToken.CompletionToken.Event
      );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = gBS->CreateEvent(
      EVT_NOTIFY_SIGNAL,
      TPL_CALLBACK,
      RxReadCallback,
      Read,
      &Read->RxIoToken.CompletionToken.Event
      );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  TxRead = AllocateZeroPool (sizeof (P9TRead));
  if (TxRead == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  TxRead->Header.Size = sizeof (P9TRead);
  TxRead->Header.Id   = Tread;
  TxRead->Header.Tag  = Volume->Tag;
  TxRead->Fid         = IFile->Fid;
  TxRead->Offset      = IFile->Position;
  TxRead->Count       = *Count;

  Read->IsTxDone = FALSE;
  Status = TransmitTcp4 (
      Tcp4,
      &Read->TxIoToken,
      TxRead,
      sizeof (P9TRead)
      );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  while (!Read->IsTxDone) {
    Tcp4->Poll (Tcp4);
  }

  RxReadSize = sizeof (P9RRead) + *Count;
  RxRead = AllocateZeroPool (RxReadSize);
  if (RxRead == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  CurrentRxRead = RxRead;
  CurrentRxReadSize = *Count;
  HasMoreData = TRUE;
  while (HasMoreData == TRUE) {
    Read->IsRxDone = FALSE;
    Status = ReceiveTcp4 (
        Tcp4,
        &Read->RxIoToken,
        CurrentRxRead,
        P9_MSIZE
        );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "%a:%d: %r\n", __func__, __LINE__, Status));
      goto Exit;
    }

    while (!Read->IsRxDone) {
      Tcp4->Poll (Tcp4);
    }
    CurrentDataLength = Read->RxIoToken.Packet.RxData->DataLength;
    CurrentRxRead = (UINT8 *)CurrentRxRead + CurrentDataLength;
    if (CurrentDataLength < CurrentRxReadSize) {
      HasMoreData = TRUE;
      CurrentRxReadSize -= CurrentDataLength;
    } else {
      HasMoreData = FALSE;
    }
  }

  if (RxRead->Header.Id != Rread) {
    Status = P9Error (RxRead, RxReadSize);
    goto Exit;
  }

  CopyMem (Data, (VOID *)RxRead->Data, RxRead->Count);
  *Count = RxRead->Count;
  IFile->Position += RxRead->Count;

  Status = EFI_SUCCESS;

Exit:
  gBS->CloseEvent (Read->RxIoToken.CompletionToken.Event);

  if (Read != NULL) {
    FreePool (Read);
  }

  if (TxRead != NULL) {
    FreePool (TxRead);
  }

  if (RxRead != NULL) {
    FreePool (RxRead);
  }

  return Status;
}