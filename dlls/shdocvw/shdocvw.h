/*
 * Copyright 2001 John R. Sheets (for CodeWeavers)
 * Copyright 2002 Hidenori Takeshima
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef WINE_SHDOCVW_H
#define WINE_SHDOCVW_H

#include "comimpl.h"

HRESULT CWebBrowserImpl_AllocObj(IUnknown* punkOuter,void** ppobj);
HRESULT CConnectionPointImpl_AllocObj(IUnknown* punkOuter,void** ppobj);


/* FIXME - move to header... */
DEFINE_GUID(IID_INotifyDBEvents,
0xdb526cc0, 0xd188, 0x11cd, 0xad, 0x48, 0x0, 0xaa, 0x0, 0x3c, 0x9c, 0xb6);


#endif	/* WINE_SHDOCVW_H */
