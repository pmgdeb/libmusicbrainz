/*____________________________________________________________________________

  MusicBrainz -- The Internet music metadatabase

  Portions Copyright (C) 2000 Relatable

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  $Id$
____________________________________________________________________________*/

/***************************************************************************
sigclient.cpp  -  description
-------------------
copyright            : (C) 2000 by Relatable 
written by           : Isaac Richards
email                : ijr@relatable.com
***************************************************************************/

#ifdef WIN32
#pragma warning(disable:4786)
#endif

#include "sigclient.h"
#ifdef WIN32
#include "wincomsocket.h"
#else
#include "comsocket.h"
#endif

#include "sigxdr.h"

namespace SigClientVars
{
    static const char cGetGUID = 'G';
    static const char cDisconnect = 'E';

    static const int nGUIDSize = 16;
    static const int nTimeout = 15;
    static const int nHeaderSize = sizeof(char) + sizeof(int);
}

using namespace std;

SigClient::SigClient()
{
    m_pSocket = new COMSocket;
    m_nNumFailures = 0;
}

SigClient::~SigClient()
{
    if (m_pSocket->IsConnected()) 
        this->Disconnect();
    if (m_pSocket != NULL) 
        delete m_pSocket;
}

int SigClient::GetSignature(AudioSig *sig, string &strGUID, 
                            string strCollectionID)
{
    int nConRes = this->Connect(m_strIP, m_nPort);
    if (nConRes != 0) 
        return -1;

    SigXDR converter;

    int nOffSet = sizeof(char) + sizeof(int);
    int nGUIDLen = strCollectionID.size() * sizeof(char) + sizeof(char);
    int iSigEncodeSize = 35 * sizeof(int32) + nGUIDLen;
    int nTotalSize = nOffSet + iSigEncodeSize;

    char* pBuffer = new char[nTotalSize];
    memset(pBuffer, 0, nTotalSize);
    memcpy(&pBuffer[0], &SigClientVars::cGetGUID, sizeof(char));
    memcpy(&pBuffer[1], &iSigEncodeSize, sizeof(int));

    iSigEncodeSize -= nGUIDLen;
    char *sigencode = converter.FromSig(sig);
    memcpy(&pBuffer[nOffSet], sigencode, iSigEncodeSize);

    memcpy(&pBuffer[nOffSet + iSigEncodeSize], strCollectionID.c_str(),
           nGUIDLen - sizeof(char));
    pBuffer[nOffSet + iSigEncodeSize + nGUIDLen - sizeof(char)] = '\0';

    int nBytes = 0;
    int ret = m_pSocket->Write(pBuffer, nTotalSize, &nBytes);

    memset(pBuffer, 0, nTotalSize);
    int iGUIDSize = 16 * sizeof(int32);

    ret = m_pSocket->NBRead(pBuffer, iGUIDSize, &nBytes, 
                            SigClientVars::nTimeout);

    
    if ((ret != -1) && (nBytes == iGUIDSize)) {
        ret = 0;
        strGUID = converter.ToStrGUID(pBuffer, nBytes);
    }
    else {
	ret = -1;
        strGUID = "";
    }
    this->Disconnect();

    delete [] pBuffer;
    delete [] sigencode;

    return ret;
}

int SigClient::Connect(string& strIP, int nPort)
{
    if (m_nNumFailures > 5) 
        return -1;  // server probably down. 
    int nErr = m_pSocket->Connect(strIP.c_str(), nPort, SOCK_STREAM);
    if (nErr == -1)
    {
        m_nNumFailures++;
        return -1;
    }
    m_nNumFailures = 0;
    return 0;
}

int SigClient::Disconnect()
{
    if (m_pSocket->IsConnected())
    {
        char cBuffer[sizeof(char) + sizeof(int)];
        memset(cBuffer, 0, sizeof(char) + sizeof(int));
        cBuffer[0] = SigClientVars::cDisconnect;

        int nBytes = 0;
        m_pSocket->Write(cBuffer, sizeof(char) + sizeof(int), 
                         &nBytes);
        m_pSocket->Disconnect();	
    }
	
    return 0;
}
