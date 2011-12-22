/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
 *   Ali Juma <ajuma@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CompositorParent.h"
#include "ShadowLayersParent.h"
#include "LayerManagerOGL.h"

namespace mozilla {
namespace layers {

CompositorParent::CompositorParent()
  : mStopped(false)
{

  MOZ_COUNT_CTOR(CompositorParent);
}

CompositorParent::~CompositorParent()
{
  MOZ_COUNT_DTOR(CompositorParent);
}

void
CompositorParent::Destroy()
{
  size_t numChildren = ManagedPLayersParent().Length();
  NS_ABORT_IF_FALSE(0 == numChildren || 1 == numChildren,
                    "compositor must only have 0 or 1 layer manager");

  if (numChildren) {
    ShadowLayersParent* layers =
      static_cast<ShadowLayersParent*>(ManagedPLayersParent()[0]);
    layers->Destroy();
  }
}

bool
CompositorParent::RecvInit()
{
  CancelableTask *composeTask = NewRunnableMethod(this, &CompositorParent::Composite);
  MessageLoop::current()->PostTask(FROM_HERE, composeTask);
  return true;
}

bool
CompositorParent::RecvStop()
{
  mStopped = true;
  Destroy();
  return true;
}

void
CompositorParent::RequestComposition()
{
  CancelableTask *composeTask = NewRunnableMethod(this, &CompositorParent::Composite);
  MessageLoop::current()->PostTask(FROM_HERE, composeTask);
}

void
CompositorParent::Composite()
{
  if (mStopped) {
    return;
  }

  if (!mLayerManager) {
    CancelableTask *composeTask = NewRunnableMethod(this, &CompositorParent::Composite);
    MessageLoop::current()->PostDelayedTask(FROM_HERE, composeTask, 10);
    return;
  }

  mLayerManager->EndEmptyTransaction();
}

PLayersParent*
CompositorParent::AllocPLayers(const LayersBackend &backend, const WidgetDescriptor &widget)
{
  if (widget.type() != WidgetDescriptor::TViewWidget) {
    NS_ERROR("Invalid widget descriptor\n");
    return NULL;
  }

  if (backend == LayerManager::LAYERS_OPENGL) {
    nsIWidget *nsWidget = (nsIWidget*)widget.get_ViewWidget().widgetPtr();
    nsRefPtr<LayerManagerOGL> layerManager = new LayerManagerOGL(nsWidget);

    if (!layerManager->Initialize()) {
      NS_ERROR("Failed to init OGL Layers");
      return NULL;
    }

    ShadowLayerManager* slm = layerManager->AsShadowManager();
    if (!slm) {
      return NULL;
    }

    void *glContext = layerManager->gl()->GetNativeData(mozilla::gl::GLContext::NativeGLContext);
    NativeContext nativeContext = NativeContext((uintptr_t)glContext);
    SendNativeContextCreated(nativeContext);

    mLayerManager = layerManager;

    return new ShadowLayersParent(slm, this);
  } else {
    NS_ERROR("Unsupported backend selected for Async Compositor");
    return NULL;
  }
}

bool
CompositorParent::DeallocPLayers(PLayersParent* actor)
{
  delete actor;
  return true;
}

} // namespace layers
} // namespace mozilla

