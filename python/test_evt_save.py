# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    check I/O of own event format file

"""

from ebiv_utils import EBIDataSet
import os

# verify part of the contents...
def verifyContents(ev1,ev2, idx0, N):
    for i in range(idx0,idx0+N):
        print( '%5d  t1:%8d t2:%8d  x1 %4d x2 %4d  y1 %4d y2 %4d  p1:%d p2:%d' \
          % (i, ev1.evData['t'][i], ev2.evData['t'][i], \
             ev1.evData['x'][i], ev2.evData['x'][i], \
             ev1.evData['y'][i], ev2.evData['y'][i], \
             ev1.evData['p'][i], ev2.evData['p'][i] \
             ))

dataDir = '../sample_data/'
fileStub = 'wallflow4_dense_3'
fnRAW = dataDir + fileStub + '.raw'
plotDir = dataDir + 'plots/'
destDir = dataDir

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)

#%% Load events from RAW-format event file and compute variance map
ev1 = EBIDataSet(fnRAW, dbg=1)
imgH,imgW = ev1.imageSize()

# save in own evt-format
fnOUT = dataDir + fileStub + '.evt'
ev1.saveEvents(fnOUT)

# try reading back
ev2 = EBIDataSet(fnOUT, dbg=1)
verifyContents(ev1,ev2, 12345, 10)
    
# save 100 ms worth of events
fnOUT_100ms = dataDir + fileStub + '_100ms.evt'
ev2.saveEvents(fnOUT_100ms, offset=0, duration=100000)
# try reading back
ev3 = EBIDataSet(fnOUT_100ms, dbg=1)
verifyContents(ev2,ev3, 12345, 10)

