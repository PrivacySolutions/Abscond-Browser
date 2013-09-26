/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdarg.h>

#include "prprf.h"

#include "nsIServiceManager.h"

#include "nsIConsoleService.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIHTMLCollection.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsIPrincipal.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"
#include "mozilla/gfx/Matrix.h"

using namespace mozilla::gfx;

#include "nsIScriptObjectPrincipal.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"

#define TOPIC_CANVAS_PERMISSIONS_PROMPT "canvas-permissions-prompt"
#define PERMISSION_CANVAS_EXTRACT_DATA "canvas/extractData"

namespace mozilla {
namespace CanvasUtils {

// Check site-specific permission and display prompt if appropriate.
bool
IsImageExtractionAllowed(nsIDocument *aDocument)
{
  if (!aDocument)
    return false;

  nsPIDOMWindow *win = aDocument->GetWindow();
  nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(win));
  if (sop && nsContentUtils::IsSystemPrincipal(sop->GetPrincipal()))
    return true;

  bool isAllowed = false;
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
                                do_GetService(THIRDPARTYUTIL_CONTRACTID);
  nsCOMPtr<nsIPermissionManager> permissionManager =
                          do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (thirdPartyUtil && permissionManager) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = thirdPartyUtil->GetFirstPartyURI(NULL, aDocument,
                                                   getter_AddRefs(uri));
    uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
    if (NS_SUCCEEDED(rv)) {
      // Allow local files to access canvas data; check content permissions
      // for remote pages.
      bool isFileURL = false;
      (void)uri->SchemeIs("file", &isFileURL);
      if (isFileURL)
        permission = nsIPermissionManager::ALLOW_ACTION;
      else {
        rv = permissionManager->TestPermission(uri,
                                PERMISSION_CANVAS_EXTRACT_DATA, &permission);
      }
    }

    if (NS_SUCCEEDED(rv)) {
      isAllowed = (permission == nsIPermissionManager::ALLOW_ACTION);

      if (!isAllowed && (permission != nsIPermissionManager::DENY_ACTION)) {
        // Send notification so that a prompt is displayed.
        nsCString spec;
        rv = uri->GetSpec(spec);
        NS_ENSURE_SUCCESS(rv, false);
        nsCOMPtr<nsIObserverService> obs =
                                    mozilla::services::GetObserverService();
        obs->NotifyObservers(win, TOPIC_CANVAS_PERMISSIONS_PROMPT,
                             NS_ConvertUTF8toUTF16(spec).get());
      }
    }
  }

  return isAllowed;
}

void
DoDrawImageSecurityCheck(dom::HTMLCanvasElement *aCanvasElement,
                         nsIPrincipal *aPrincipal,
                         bool forceWriteOnly,
                         bool CORSUsed)
{
    NS_PRECONDITION(aPrincipal, "Must have a principal here");

    // Callers should ensure that mCanvasElement is non-null before calling this
    if (!aCanvasElement) {
        NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
        return;
    }

    if (aCanvasElement->IsWriteOnly())
        return;

    // If we explicitly set WriteOnly just do it and get out
    if (forceWriteOnly) {
        aCanvasElement->SetWriteOnly();
        return;
    }

    // No need to do a security check if the image used CORS for the load
    if (CORSUsed)
        return;

    // Ignore document.domain in this check.
    bool subsumes;
    nsresult rv =
        aCanvasElement->NodePrincipal()->SubsumesIgnoringDomain(aPrincipal,
                                                                &subsumes);

    if (NS_SUCCEEDED(rv) && subsumes) {
        // This canvas has access to that image anyway
        return;
    }

    aCanvasElement->SetWriteOnly();
}

bool
CoerceDouble(JS::Value v, double* d)
{
    if (JSVAL_IS_DOUBLE(v)) {
        *d = JSVAL_TO_DOUBLE(v);
    } else if (JSVAL_IS_INT(v)) {
        *d = double(JSVAL_TO_INT(v));
    } else if (JSVAL_IS_VOID(v)) {
        *d = 0.0;
    } else {
        return false;
    }
    return true;
}

} // namespace CanvasUtils
} // namespace mozilla
