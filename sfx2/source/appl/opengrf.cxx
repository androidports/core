/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */


#include <tools/urlobj.hxx>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/ui/dialogs/CommonFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <com/sun/star/ui/dialogs/ExtendedFilePickerElementIds.hpp>
#include <com/sun/star/ui/dialogs/FilePreviewImageFormats.hpp>
#include <com/sun/star/ui/dialogs/ListboxControlActions.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerControlAccess.hpp>
#include <com/sun/star/ui/dialogs/XFilePicker.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerListener.hpp>
#include <com/sun/star/ui/dialogs/XFilePickerNotifier.hpp>
#include <com/sun/star/ui/dialogs/XFilePreview.hpp>
#include <com/sun/star/ui/dialogs/XFilterManager.hpp>
#include <o3tl/any.hxx>
#include <svl/urihelper.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <svtools/transfer.hxx>
#include <sot/formats.hxx>
#include <vcl/msgbox.hxx>
#include <sfx2/filedlghelper.hxx>
#include <sfx2/docfile.hxx>
#include <unotools/pathoptions.hxx>
#include <sfx2/opengrf.hxx>
#include <sfx2/strings.hrc>
#include <sfx2/sfxresid.hxx>


using namespace ::com::sun::star;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::ui::dialogs;
using namespace ::com::sun::star::uno;
using namespace ::cppu;

const char* SvxOpenGrfErr2ResId( ErrCode err )
{
    if (err == ERRCODE_GRFILTER_OPENERROR)
        return RID_SVXSTR_GRFILTER_OPENERROR;
    else if (err == ERRCODE_GRFILTER_IOERROR)
        return RID_SVXSTR_GRFILTER_IOERROR;
    else if (err == ERRCODE_GRFILTER_VERSIONERROR)
        return RID_SVXSTR_GRFILTER_VERSIONERROR;
    else if (err == ERRCODE_GRFILTER_FILTERERROR)
        return RID_SVXSTR_GRFILTER_FILTERERROR;
    else
        return RID_SVXSTR_GRFILTER_FORMATERROR;
}

struct SvxOpenGrf_Impl
{
    SvxOpenGrf_Impl(const vcl::Window* pPreferredParent);

    sfx2::FileDialogHelper                  aFileDlg;
    uno::Reference < XFilePickerControlAccess > xCtrlAcc;
};


SvxOpenGrf_Impl::SvxOpenGrf_Impl(const vcl::Window* pPreferredParent)
    : aFileDlg(ui::dialogs::TemplateDescription::FILEOPEN_LINK_PREVIEW,
            FileDialogFlags::Graphic, pPreferredParent)
{
    uno::Reference < XFilePicker3 > xFP = aFileDlg.GetFilePicker();
    xCtrlAcc.set(xFP, UNO_QUERY);
}


SvxOpenGraphicDialog::SvxOpenGraphicDialog(const OUString& rTitle, const vcl::Window* pPreferredParent)
    : mpImpl(new SvxOpenGrf_Impl(pPreferredParent))
{
    mpImpl->aFileDlg.SetTitle(rTitle);
}

SvxOpenGraphicDialog::~SvxOpenGraphicDialog()
{
}

ErrCode SvxOpenGraphicDialog::Execute()
{
    ErrCode nImpRet;
    bool    bQuitLoop(false);

    while( !bQuitLoop &&
           mpImpl->aFileDlg.Execute() == ERRCODE_NONE )
    {
        if( !GetPath().isEmpty() )
        {
            GraphicFilter& rFilter = GraphicFilter::GetGraphicFilter();
            INetURLObject aObj( GetPath() );

            // check whether we can load the graphic
            OUString  aCurFilter( GetCurrentFilter() );
            sal_uInt16  nFormatNum = rFilter.GetImportFormatNumber( aCurFilter );
            sal_uInt16  nRetFormat = 0;
            sal_uInt16  nFound = USHRT_MAX;

            // non-local?
            if ( INetProtocol::File != aObj.GetProtocol() )
            {
                SfxMedium aMed( aObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ), StreamMode::READ );
                aMed.Download();
                SvStream* pStream = aMed.GetInStream();

                if( pStream )
                    nImpRet = rFilter.CanImportGraphic( aObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ), *pStream, nFormatNum, &nRetFormat );
                else
                    nImpRet = rFilter.CanImportGraphic( aObj, nFormatNum, &nRetFormat );

                if ( ERRCODE_NONE != nImpRet )
                {
                    if ( !pStream )
                        nImpRet = rFilter.CanImportGraphic( aObj, GRFILTER_FORMAT_DONTKNOW, &nRetFormat );
                    else
                        nImpRet = rFilter.CanImportGraphic( aObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ), *pStream,
                                                             GRFILTER_FORMAT_DONTKNOW, &nRetFormat );
                }
            }
            else
            {
                if( (nImpRet=rFilter.CanImportGraphic( aObj, nFormatNum, &nRetFormat )) != ERRCODE_NONE )
                    nImpRet = rFilter.CanImportGraphic( aObj, GRFILTER_FORMAT_DONTKNOW, &nRetFormat );
            }

            if ( ERRCODE_NONE == nImpRet )
                nFound = nRetFormat;

            // could not load?
            if ( nFound == USHRT_MAX )
            {
                ScopedVclPtrInstance< WarningBox > aWarningBox(nullptr, MessBoxStyle::RetryCancel, WB_3DLOOK, SfxResId( SvxOpenGrfErr2ResId(nImpRet) ));
                bQuitLoop = aWarningBox->Execute() != RET_RETRY;
            }
            else
            {
                // setup appropriate filter (so next time, it will work)
                if( rFilter.GetImportFormatCount() )
                {
                    OUString  aFormatName(rFilter.GetImportFormatName(nFound));
                    SetCurrentFilter(aFormatName);
                }

                return nImpRet;
            }
        }
    }

    // cancel
    return ErrCode(sal_uInt32(-1));
}


void SvxOpenGraphicDialog::SetPath( const OUString& rPath, bool bLinkState )
{
    mpImpl->aFileDlg.SetDisplayDirectory(rPath);
    AsLink(bLinkState);
}


void SvxOpenGraphicDialog::EnableLink( bool state )
{
    if( mpImpl->xCtrlAcc.is() )
    {
        try
        {
            mpImpl->xCtrlAcc->enableControl( ExtendedFilePickerElementIds::CHECKBOX_LINK, state );
        }
        catch(const IllegalArgumentException&)
        {
#ifdef DBG_UTIL
            OSL_FAIL( "Cannot enable \"link\" checkbox" );
#endif
        }
    }
}


void SvxOpenGraphicDialog::AsLink(bool bState)
{
    if( mpImpl->xCtrlAcc.is() )
    {
        try
        {
            mpImpl->xCtrlAcc->setValue( ExtendedFilePickerElementIds::CHECKBOX_LINK, 0, Any(bState) );
        }
        catch(const IllegalArgumentException&)
        {
#ifdef DBG_UTIL
            OSL_FAIL( "Cannot check \"link\" checkbox" );
#endif
        }
    }
}


bool SvxOpenGraphicDialog::IsAsLink() const
{
    try
    {
        if( mpImpl->xCtrlAcc.is() )
        {
            Any aVal = mpImpl->xCtrlAcc->getValue( ExtendedFilePickerElementIds::CHECKBOX_LINK, 0 );
            DBG_ASSERT(aVal.hasValue(), "Value CBX_INSERT_AS_LINK not found");
            return aVal.hasValue() && *o3tl::doAccess<bool>(aVal);
        }
    }
    catch(const IllegalArgumentException&)
    {
#ifdef DBG_UTIL
        OSL_FAIL( "Cannot access \"link\" checkbox" );
#endif
    }

    return false;
}


ErrCode SvxOpenGraphicDialog::GetGraphic(Graphic& rGraphic) const
{
    return mpImpl->aFileDlg.GetGraphic(rGraphic);
}


OUString SvxOpenGraphicDialog::GetPath() const
{
    return mpImpl->aFileDlg.GetPath();
}


OUString SvxOpenGraphicDialog::GetCurrentFilter() const
{
    return mpImpl->aFileDlg.GetCurrentFilter();
}


void SvxOpenGraphicDialog::SetCurrentFilter(const OUString& rStr)
{
    mpImpl->aFileDlg.SetCurrentFilter(rStr);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
