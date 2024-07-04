# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    various utilities for handling event-based imaging data (EBI)
"""
import os
import numpy as np
from struct import unpack,pack

def EBILoadEvents(
        fnIN:str,       # input file
        dbg = False
        ):
    """
    Load events from file
 
    Parameters
    ----------
    fnIN : str
        full path of file with binary event data in simplified 64-bit format
    dbg : bool, optional
        produce debugging output during execution. The default is False.

    Raises
    ------
    IOError
        Missing or invalid file.

    Returns
    -------
    dict
        numpy arrays: 
            event time 't' in [usec], 
            event position 'x' in [pixel], 
            event position 'y' in [pixel], 
            event polarity 'p' [0,1]
        'time_stamp' of data set in [usec]
        'image_size' of sensor (height,width) in [pixel]

    """
    if not os.path.isfile(fnIN):
        raise IOError('File not found: ' + fnIN)
    # open as binary file
    with open(fnIN, "rb") as binary_file:
        binary_file.seek(0, 2)  # Seek the end
        #num_bytes = binary_file.tell()  # Get the file size
        #print('file size: '+str(num_bytes))
        binary_file.seek(0)
        header = binary_file.read( 64 )
        sig, fileSize, eventCnt, timeStamp, duration, hdrLen, w, h =\
            unpack('4s3Q4L',header)
        if not (sig == b'EVT3'):
            raise IOError('Not an event file')
        if dbg:
            print("signature:      " + str(sig))
            print("file size:      " + str(fileSize))
            print("header lenth:   " + str(hdrLen))
            print("event count:    " + str(eventCnt))
            print("duration [us]:  " + str(duration))
            print("timeStamp [us]: " + str(timeStamp))
            print("image size [h,w]" + str([h,w]))
        
        binary_file.seek(hdrLen)
        dataIN = np.fromfile(binary_file, 
                             dtype=np.uint32, count=(eventCnt*2)).reshape(eventCnt,2)
        binary_file.close()
        
        # unpack event data
        evTime = (dataIN[:,1] >> 1)
        evY = (dataIN[:,0] >> 16)
        evX = (dataIN[:,0] & 0xFFFF)
        evPol = (dataIN[:,1] & 0x1)
        # return as dict
        return {'t':evTime, 'x':evX, 'y':evY, 'p':evPol, \
                'time_stamp':timeStamp, 'image_size':[h,w] }
        #return ev, [height, width]
        
def EBISaveEvents(fnOUT:str, evDict, offset = 0, duration = 0,):
    """
    Save the current event set as binary in 64-bit evt-format [xypt] 
    [t 31bit][p 1bit][x 16bit][y 16bit]
    """
    # evDict = {'t':evTime, 'x':evX, 'y':evY, 'p':evPol, \
    #        'time_stamp':timeStamp, 'image_size':[h,w] }
    t1 = offset
    t2 = t1 + duration
    t_max = evDict['t'].max() + 1
    if duration <=0:
        t2 = t_max
    timIdx = np.where((evDict['t'] >= t1) & (evDict['t'] <= t2))
    evTime_OUT = evDict['t'][timIdx]
    evX_OUT = evDict['x'][timIdx]
    evY_OUT = evDict['y'][timIdx]
    evPol_OUT = evDict['p'][timIdx]
    eventCnt = evTime_OUT.size
    packedData = (evX_OUT) | (evY_OUT << 16) \
            | (evTime_OUT.astype(np.int64) << 33) \
                | (evPol_OUT.astype(np.int64) << 32)
    # header
    h,w = evDict['image_size']
    timeStamp = evDict['time_stamp']
    duration = evTime_OUT.max() - t1
    sig = b'EVT3'
    hdrLen = 64
    fileSize = hdrLen + (eventCnt*8)
    fillVal = 0
    header = pack('4s3Q4L2Q', sig, fileSize, eventCnt, timeStamp, duration, 
                  hdrLen, w, h, fillVal,fillVal)
    with open(fnOUT, "wb") as binary_file:
        nBytesWritten = binary_file.write( header )
        nBytesWritten2 = binary_file.write( packedData )
        binary_file.close()
        print('Wrote %d events of duration %gms into EVT file with (%d+%d) bytes' \
              % (eventCnt, (duration/1000), nBytesWritten, nBytesWritten2))
        return True
    return False

def EBILoadRawEvents(
        fnRAW,      # input file
        #duration = 0, #2000000.0,   # in usec
        #offset = 0, # time to skip from the beginning
        getInfo=False,
        dbg = False,
        ):
    """
    Load events from Prophesee's RAW file format
    Requires pyEBIV package
    """
    from pyebiv import EBIV
    if not os.path.isfile(fnRAW):
        print('ERROR - file not found: ' + fnRAW)
        return -1
    if dbg:
        print("Loading events from: " + fnRAW )
    evData = EBIV(fnRAW)
    imgH,imgW = evData.sensorSize()
    if dbg:
        print('Sensor size: ' + str([imgH,imgW]))
    numEvents = evData.eventCount()
    if dbg:
        print('Number of events: ' + str(numEvents))
    timeStamp = evData.timeStamp()
    if dbg:
        print('Time-stamp: ' + str(timeStamp))
    # retrieve event data
    evts = np.array(evData.events()).reshape(evData.eventCount(),4)
    if dbg:
        print('size of evts: ' + str(evts.shape))
    return {'t':evts[:,0], 'x':evts[:,1], 'y':evts[:,2], 'p':evts[:,3], \
            'time_stamp':timeStamp, 'image_size':[imgH,imgW] }


def EBILoadRawEventsMetaVision(
        fnRAW,      # input file
        duration = 0, #2000000.0,   # in usec
        offset = 0, # time to skip from the beginning
        getInfo=False,
        dbg = False,
        ):
    """
    Load events from Prophesee's RAW file format
    Requires installation of MetaVision package from Prophesee
    """
    from metavision_core.event_io import RawReader
    if not os.path.isfile(fnRAW):
        print('ERROR - file not found: ' + fnRAW)
        return -1

    mv_raw = RawReader(fnRAW)
    
    height, width = mv_raw.get_size()
    print("Image size: W %d x H %d" % (width, height)) #== (640, 480)
    #print("Duration: %g s" % (mv_raw.duration_s() )) #== (640, 480)
    
    # get info on length of file
    if duration <= 0:
        count = 0
        n_events = 10000
        while not mv_raw.is_done():
            evs = mv_raw.load_n_events(n_events)
            count += len(evs)
            duration = evs['t'][-1]
        #info = {'duration': int(duration), 'count': int(count)}
        print('File length: %g s with %d events' %(duration/1e6, count))
        #mv_raw.close()
    
    mv_raw = RawReader(fnRAW)
    ev = mv_raw.load_delta_t(offset)    # discard initial events
    ev = mv_raw.load_delta_t(duration) 
    print("Loaded %d events for %g ms" % (ev.size, duration/1000))
    ev['t'] -= ev['t'].min()    # subtract initial offset
    return ev, [height, width]


class EBISubVolume(object):
    """
    Container for sampled event within specific spatial and temporal boundaries
    """
    def __init__(self, sample=None,):
        self.evData={
            'time':np.array([],dtype=np.float32), 
            'x':np.array([]), 
            'y':np.array([]), 
            'roiX':[0, 0],
            'roiY':[0, 0],
            'roiTime':[0,0],
            }
        self.centroid = [0.,0]
        self.warpData = np.array([],dtype=np.float32)
        # self.evTime = np.array([],dtype=np.float32) 
        # self.evX = np.array([],dtype=np.float32) 
        # self.evY = np.array([],dtype=np.float32) 
        # self.roi = [0, 0, 0, 0, 0,0]

        if sample != None:
            self.evData = sample
            roiW = sample['roiX'][1] - sample['roiX'][0]
            roiH = sample['roiY'][1] - sample['roiY'][0]
            self.warpData = np.zeros([roiH,roiW],dtype=np.float32)
            self._calcCentroid()

    def _calcCentroid(self):
        self.centroid = [\
            (self.evData['roiX'][1] + self.evData['roiX'][0])/2, \
            (self.evData['roiY'][1] + self.evData['roiY'][0])/2  \
            ]
        
    def renderSample(self):
        if self.evData['time'].size == 0:
            return np.array([],dtype=np.float32)
        roiW = (self.evData['roiX'][1] - self.evData['roiX'][0])
        roiH = (self.evData['roiY'][1] - self.evData['roiY'][0])
        sumImg = np.zeros((roiH,roiW))
        sumImg[np.int32(self.evData['y']),\
               np.int32(self.evData['x'])] = self.evData['time'] 
        return sumImg
    def warpedSample(self):
        if self.evData['time'].size == 0:
            return np.array([],dtype=np.float32)
        return self.warpData

    def asVolume(self, nt:int):
        if self.evData['time'].size == 0:
            return np.array([],dtype=np.float32)
        nx = (self.evData['roiX'][1] - self.evData['roiX'][0])
        ny = (self.evData['roiY'][1] - self.evData['roiY'][0])
        dt = self.evData['roiTime'][1]-self.evData['roiTime'][0]
        vol3d = np.zeros([nt*ny*nx])
        # produces indices in time
        t_idx = np.floor((self.evData['time'])*nt / dt).astype(int)
        x_idx = self.evData['x'].astype(int)
        y_idx = self.evData['y'].astype(int)
        # idx3d = (t_idx) x (nx*ny) + (y_idx) * nx + x_idx
        idx3d = ((t_idx * ny) + y_idx) * nx + x_idx
        vol3d = np.zeros([nt*ny*nx])
        vol3d[idx3d] = 1    # mark events within 3-D volume
        return vol3d.reshape(nt,ny,nx)
            
    def sumWarpedEvents(self, \
                  velX:float,   # in [px/us]
                  velY:float,   # in [px/us]
                  interpolation='nearest', #useNearest = True,
                  objectiveFxn='var',
                  dbg=False,
                  ):
        ''' 
        Compute image of warped events (IWE) for given velocity (velX,velY) and then
        apply objective function on the IWE.
        Makes use of various objective ("reward") functions as suggested by Stoffregen & Kleeman (2019)
        "Event Cameras, Contrast Maximization and Reward Functions: an Analysis", CVPR19
        
        Implemented objective functions:
            'var' - variance of warped events
            'SumSq' - sum of squares
            'SumExp' - sum of exponetials
            'R1'
            'ISOA' - inverse sum of accumulations 
            
        Returns [mean of warped events, 
                 value of objective fxn for warped events, 
                 events used]
        '''
        vx = -velX
        vy = -velY
        # pos = vel * time --> vel = pos / time
        # warp with respect to center in time
        t_mean = (self.evData['roiTime'][1]-self.evData['roiTime'][0]) /2 
        #print('t_mean: ' + str(t_mean))
        warpX = (self.evData['x'])+((self.evData['time']-t_mean)*vx)
        warpY = (self.evData['y'])+((self.evData['time']-t_mean)*vy)
        # print('warpX: %g %g' %(warpX.min(),warpX.max()))
        # print('warpY: %g %g' %(warpY.min(),warpY.max()))
        # after warping the events are no longer pixel-centered and must be projected 
        # onto output grid 
        [roiH,roiW] = self.warpData.shape
        self.warpData[:,:] = 0
        cnt = 0
        if interpolation in ['nearest']:  # faster than interpolation
            pixX = np.int32(np.round(warpX))  # i pixel
            pixY = np.int32(np.round(warpY))  # j pixel
            for px,py in zip(pixX,pixY):
                if (px>= 0) and (py>=0) and (px<(roiW)) and (py<(roiH)):
                    self.warpData[py  ,px  ] += 1
                    cnt += 1    # number of events used
            if(dbg):
                print(pixX)
        else:
            # we use bilinear interpolation here - slower
            pixL = np.int32(np.floor(warpX))  # left pixel
            pixT = np.int32(np.floor(warpY))  # top pixel
            wghtL = warpX - pixL    # weighting factor
            wghtR = 1.0 - wghtL
            wghtT = warpY - pixT
            wghtB = 1.0 - wghtT
            wghtTL = wghtT * wghtL
            wghtBL = wghtB * wghtL
            wghtTR = wghtT * wghtR
            wghtBR = wghtB * wghtR
            for px,py, wTL,wTR,wBL,wBR in zip(pixL,pixT,wghtTL,wghtTR,wghtBL,wghtBR):
                if (px>= 0) and (py>=0) and (px<(roiW-1)) and (py<(roiH-1)):
                    self.warpData[py  ,px  ] += wTL
                    self.warpData[py  ,px+1] += wTR
                    self.warpData[py+1,px  ] += wBL
                    self.warpData[py+1,px+1] += wBR
                    cnt += 1    # number of events used
        
        # compute reward function
        mean = self.warpData.mean()
        if objectiveFxn.lower() in ['var', 'variance']:
            fxnVal = self.warpData.var()
        # Sum of squares
        elif objectiveFxn.lower() in ['sumsq', 'sum_of_squares']:
            fxnVal = (self.warpData**2).sum()
        # Sum of exponentials
        elif objectiveFxn.lower() in ['sumexp', 'sum_of_exponentials']:
            fxnVal = np.exp(self.warpData).sum()
        # R1 ((Stoffregen et al, Event Cameras, Contrast Maximization and 
        # Reward Functions: an Analysis, CVPR19)
        elif objectiveFxn.lower() in ['r1']:
            p = 3
            sos = np.mean(self.warpData*self.warpData)
            exp = np.exp(-p*self.warpData.astype(np.double))
            sosa = np.sum(exp)
            fxnVal = sos*sosa
        elif objectiveFxn.lower() in ['isoa']:
            # inverse sum of accumulations 
            thresh = 1
            fxnVal = np.sum(np.where(self.warpData>thresh, 1, 0))
        elif objectiveFxn.lower() in ['moa']:
            fxnVal = np.max(self.warpData)
        else:
            raise ValueError('Reward-function not specified')
        eventsUsed = cnt

        return mean, fxnVal, eventsUsed

class EBIDataSet(object):
    """
    Container for event data; allows reading, writing and sampling
    """
    def __init__(self, rawFile='', dbg=False, offset=0, duration=100000):
        self.evData = []
        #self.szImg = [0,0]
        self.dbg = dbg
        
        self.currentSample = EBISubVolume() 
        
        # load events based on file extension
        if os.path.isfile(rawFile):
            base, ext = os.path.splitext(rawFile)
            if ext in ['.evt']:
                self.evData = EBILoadEvents(rawFile, dbg=dbg, )# offset=offset, duration=duration)
            elif ext in ['.raw']:
                self.evData = EBILoadRawEvents(rawFile, dbg=dbg, )# offset=offset, duration=duration)
            #self.szImg = self.evData['image_size']
            
    def loadEvents(self, rawFile:str, offset=0, duration=100000):
        self.evData = EBILoadEvents(rawFile, dbg=self.dbg, ) # offset=offset, duration=duration)
        #self.szImg = self.evData['image_size']
        return (self.evData['p'].size > 0)
    
    def saveEvents(self, fnOUT:str, offset=0, duration=0):
        #evDict = {'t':evTime, 'x':evX, 'y':evY, 'p':evPol, \
        #        'time_stamp':timeStamp, 'image_size':[h,w] }
        return EBISaveEvents(fnOUT, self.evData,
                             offset=offset, duration=duration)
    
    def loadRawEvents(self, rawFile:str, offset=0, duration=100000):
        self.evData = EBILoadRawEvents(rawFile, dbg=self.dbg,
                                       #offset=offset, duration=duration,
                                       )
        return (self.evData['p'].size > 0)
    
    def imageSize(self):
        if len(self.evData) == 0:
            return [0,0]
        if self.evData['p'].size > 0:
            return self.evData['image_size']
        
    def timeSlice(self,timeRange):
        """
        return a time-slice of the current event data set
        """
        t1 = min(timeRange)
        t2 = max(timeRange)
        evTimeIN = self.evData['t']
        timIdx = np.where((evTimeIN >= t1) & (evTimeIN < t2))
        evTime_OUT = np.float32(self.evData['t'][timIdx])
        evPol_OUT = np.float32(self.evData['p'][timIdx])
        evX_OUT = np.float32(self.evData['x'][timIdx])
        evY_OUT = np.float32(self.evData['y'][timIdx])
        return evTime_OUT, evX_OUT, evY_OUT, evPol_OUT
        
    def sampleEvents(self, 
                     posXY, 
                     szWinWH, 
                     timeRange, 
                     polarity:str='pos'     # ['pos','neg', 'both']
                     ):
        """
        return a sub-set of events for given spatial and temporal bounds
        """
        t1 = min(timeRange)
        t2 = max(timeRange)
        roiX1 = posXY[0] 
        roiX2 = posXY[0] + szWinWH[0]
        roiY1 = posXY[1] 
        roiY2 = posXY[1] + szWinWH[1]
        evTimeIN = self.evData['t']
        evPolarity = self.evData['p']
        evX = self.evData['x']
        evY = self.evData['y']
        evPol = np.array([])
        if polarity in ['pos', 'positive', '+']:
            # positive events within space-time sub-volume
            evPol = np.where((evPolarity>0) & (evTimeIN >= t1) & (evTimeIN < t2)\
                       & (evX >= roiX1) & (evX < roiX2) \
                       & (evY >= roiY1) & (evY < roiY2))
        elif polarity in ['neg', 'negative', '-']:
            # negative events within space-time sub-volume
            evPol = np.where((evPolarity==0) & (evTimeIN >= t1) & (evTimeIN < t2)\
                       & (evX >= roiX1) & (evX < roiX2) \
                       & (evY >= roiY1) & (evY < roiY2))
        elif polarity in ['both', 'all']:
            # extract all events regardless of polarity
            evPol = np.where( (evTimeIN >= t1) & (evTimeIN < t2)\
                       & (evX >= roiX1) & (evX < roiX2) \
                       & (evY >= roiY1) & (evY < roiY2))
        else:
            raise ValueError('Invalid value for polarity')
            
        evTime_OUT = np.float32(evTimeIN[evPol]) - t1
        evX_OUT = np.float32(evX[evPol])-roiX1
        evY_OUT = np.float32(evY[evPol])-roiY1

        self.currentSample = EBISubVolume(sample = {
            'time':evTime_OUT, 
            'x':evX_OUT,
            'y':evY_OUT,
            'roiX':[roiX1, roiX2], 
            'roiY':[roiY1, roiY2], 
            'roiTime':[t1,t2],
            })
            
        return self.currentSample

def CalcObjectiveForShift(VxIN, VyIN, sample, interpolation='nearest',
                    objectiveFxn='var',
                    ):
    """
    Calculate map of objective values for specified velocity candidates VxIN and VyIN
    
    See EBISubVolume.sumWarpedEvents() for details
    """
    res = []
    evList = []
    for Vy in VyIN:
        for Vx in VxIN:
            vx = Vx/1000
            vy = Vy/1000
            mean, rewardValue, evs = sample.sumWarpedEvents(
                vx,vy, interpolation=interpolation, objectiveFxn=objectiveFxn )
            res.append(rewardValue)
            evList.append(evs)
    
    mappedResult = np.array(res).reshape(VyIN.size,VxIN.size)
    eventsResult = np.array(evList).reshape(VyIN.size,VxIN.size)
    
    return mappedResult, eventsResult

def PeakFit3pt(x0:float, vals, dx, doExponentialFit=False):
    """
    Perform parabolic peak fit on 3-valued array

    Parameters
    ----------
    x0 : float
        position of brightest (central) element of vals-array
    vals : [xl,x0,xR]
        array with 3 intensity values around maximum
    dx : float
        grid spacing between values
    doExponentialFit : bool (optional) 
        fit Gaussian peak

    Returns estimated position of maximum
    """
    # sub-pixel fit
    [a,b,c] = vals[0:3] # values for pos [-1,0,+1]
    if doExponentialFit and np.all(vals[0:3] > 0):
        # compute natural log - works for positive values only
        [a,b,c] = np.log(vals[0:3])
    denom = ((a+c)*2 -b*4)
    if (denom == 0):
        return x0
    subPix = (a-c)/denom
    #print(subPix)
    return x0 + (subPix * dx)

def FindPeakInObjectiveMap(
        imgData2D, 
        eventCount2D, 
        VxIN, VyIN, 
        doExponentialFit=False,
        dbg=False
        ):
    """
    Look for highest signal in 2d array of 'imgData2D' and associate
    with pixel velocity candidate list [VxIN,VyIN].
    If possible, perform 3-point peak fit to estimate position of most probale
    pixel velocity.
    
    To fit a Gaussian peak set doExponentialFit=True
    
    Returns estimated pixel velocity [Vx,Vy], associated peak variance and event count
    """
    [ny,nx] = imgData2D.shape
    if imgData2D.var() == 0:
        return [0.,0,0,0]
    [iyMax,ixMax] = np.unravel_index(np.argmax(imgData2D), imgData2D.shape)
    Vx = VxIN[ixMax]
    Vy = VyIN[iyMax]
    dx = VxIN[1]-VxIN[0]
    dy = VyIN[1]-VyIN[0]
    maxVar = imgData2D[iyMax,ixMax]
    maxEvents = eventCount2D[iyMax,ixMax]
    if dbg:
        print("peakX: " + str(imgData2D[iyMax,ixMax-1:ixMax+2].flatten()))
        print("peakY: " + str(imgData2D[iyMax-1:iyMax+2,ixMax].flatten()))
    if(ixMax > 0) and (ixMax+1 < nx):
        Vx = PeakFit3pt(Vx, imgData2D[iyMax,ixMax-1:ixMax+2].flatten(), dx,
                        doExponentialFit=doExponentialFit)
    if(iyMax > 0) and (iyMax+1 < ny):
        Vy = PeakFit3pt(Vy, imgData2D[iyMax-1:iyMax+2,ixMax].flatten(), dy,
                        doExponentialFit=doExponentialFit)

    return [Vx,Vy,maxVar, maxEvents]

import numpy.fft
from scipy.signal import fftconvolve


def FFT_CrossCorr2D(A_in,B_in):
    """
    Computes cross-correlation between 2 data sets containing multiple 2D-planes
    (must be 3D input arrays)
    """
    A = np.copy(A_in)
    B = np.copy(B_in)
    nz,ny,nx = A.shape
    ny2 = ny*2 - 1
    nx2 = nx*2 - 1
    #corrSum = np.zeros([ny2,nx2])
    corr3D = np.zeros([nz,ny2,nx2])
    for z in range(nz):
        a = A[z,::-1, ::-1].reshape(ny,nx)
        b = B[z,::, ::].reshape(ny,nx)
        a_mean = a.mean()
        b_mean = b.mean()
        if (a_mean*b_mean>0):
            a -= a_mean
            b -= b_mean
            #corrSum += fftconvolve(B[z,:,:].reshape(ny,nx), A[z,::-1, ::-1].reshape(ny,nx))
            corr3D[z,:,:] = fftconvolve(b, a)/(a.std()*b.std())
            #corrSum += fftconvolve(b, a)/(a.std()*b.std())
    return corr3D /(nz*ny*nx)

def FindCorrPeak2D(corr, doExponentialFit=True):
    """
    Find (postive) peak in 2D correlation map
    
    Return estimated sub-pixel position [x,y] and corresponding correlation coefficient
    """
    ny,nx = corr.shape
    [iyMax,ixMax] = np.unravel_index(np.argmax(corr), corr.shape)
    dx = ixMax - nx//2
    dy = iyMax - ny//2
    if(ixMax > 0) and (ixMax+1 < nx):
        dx = PeakFit3pt(dx, corr[iyMax,ixMax-1:ixMax+2].flatten(), 1,
                        doExponentialFit=doExponentialFit)
    if(iyMax > 0) and (iyMax+1 < ny):
        dy = PeakFit3pt(dy, corr[iyMax-1:iyMax+2,ixMax].flatten(), 1,
                        doExponentialFit=doExponentialFit)
    return dx, dy, corr[iyMax,ixMax]

    
if __name__ == '__main__':
    
    # check to see if we can read data set
    dirIN = '../sample_data/'
    fileStub = 'wallflow4_dense_3'
    fnIN_RAW = dirIN + fileStub + '.raw' 
    fnIN_EVT = dirIN + fileStub + '_100ms.evt' 
    
    ebiData1 = EBIDataSet(dbg=True)
    print(ebiData1.imageSize())
    
    print("------- reading: " + fnIN_RAW)
    ebiDataRaw = EBIDataSet(fnIN_RAW, dbg=True)
    print(ebiDataRaw.imageSize())
    
    print("------- reading: " + fnIN_EVT)
    ebiDataEvt = EBIDataSet(fnIN_EVT, dbg=True)
    print(ebiDataEvt.imageSize())
    
