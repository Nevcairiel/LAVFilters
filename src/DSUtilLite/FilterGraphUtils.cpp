/*
 *      Copyright (C) 2010 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 */

#include "stdafx.h"
#include "DShowUtil.h"

HRESULT FilterGraphCleanup(IFilterGraph *pGraph)
{
  // Ugly code that removes all filters from the graph without any connected input pins
  BOOL bGraphChanged = FALSE;
  do {
    bGraphChanged = FALSE;
    // Enum Filters
    IEnumFilters *pEnum = NULL;
    IBaseFilter *pFilter;
    ULONG cFetched;
    // Fetch Enumerator
    HRESULT hr = pGraph->EnumFilters(&pEnum);
    if(FAILED(hr)) { return hr; }
    while(!bGraphChanged && pEnum->Next(1, &pFilter, &cFetched) == S_OK) {
      // Enum Pins
      IEnumPins *pPinEnum = NULL;
      if (SUCCEEDED(hr = pFilter->EnumPins(&pPinEnum))) {
        BOOL hasEmptyInput = FALSE;
        BOOL hasOutput = FALSE;
        IPin *pin;
        ULONG cPinFetched;
        while(pPinEnum->Next(1, &pin, &cPinFetched) == S_OK) {
          PIN_DIRECTION pinDir;
          // Get Pin Direction
          if (SUCCEEDED(hr = pin->QueryDirection(&pinDir))) {
            // Input pin
            if(pinDir == PINDIR_INPUT) {
              IPin *pin2;
              // Check if its connected
              if(FAILED(hr = pin->ConnectedTo(&pin2))) {
                hasEmptyInput = TRUE;
              } else {
                // if its connected, bail out
                hasEmptyInput = FALSE;
                pin2->Release();
                break;
              }
            } else if (pinDir == PINDIR_OUTPUT) {
              hasOutput = TRUE;
            }
          }
          pin->Release();
        }
        pPinEnum->Release();
        if(hasEmptyInput && hasOutput) {
          pGraph->RemoveFilter(pFilter);
          bGraphChanged = TRUE;
        }
      }
      pFilter->Release();
    }
    pEnum->Release();
  } while(bGraphChanged);
  
  return S_OK;
}