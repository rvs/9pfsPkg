/** @file
  Function that deletes a file.

Copyright (c) 2005 - 2013, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2020, Akira Moroo. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent


**/

#include "9pfs.h"

/**

  Deletes the file & Closes the file handle.

  @param  FHand                    - Handle to the file to delete.

  @retval EFI_SUCCESS              - Delete the file successfully.
  @retval EFI_WARN_DELETE_FAILURE  - Fail to delete the file.

**/
EFI_STATUS
EFIAPI
P9Delete (
  IN EFI_FILE_PROTOCOL  *FHand
  )
{
  DEBUG ((DEBUG_INFO, "%a:%d\n", __func__, __LINE__));
  return EFI_UNSUPPORTED;
}
