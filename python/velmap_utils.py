# -*- coding: utf-8 -*-
"""
@author: cewdlr

Purpose:
    A few utilities for (post)processing 2D-2C velocity data sets
"""

import numpy as np
from scipy import ndimage 
import scipy.signal as signal
from scipy.interpolate import griddata

def ComputeGradients2D(vxIN,vyIN, mode:str, \
                       bSmooth=False,
                       gridSpacingXY=[1.0,1.],
                       ):
    if(bSmooth):
        hrow = [.25,.5,.25]
        hcol = hrow #[.25,.5,.25]
        dudy, dudx = np.gradient(signal.sepfir2d(vxIN[:,:], hrow, hcol))
        dvdy, dvdx = np.gradient(signal.sepfir2d(vyIN[:,:], hrow, hcol))
    else:
        dudy, dudx = np.gradient(vxIN[:,:])
        dvdy, dvdx = np.gradient(vyIN[:,:])
    dudx /= gridSpacingXY[0];
    dvdx /= gridSpacingXY[0];
    dudy /= gridSpacingXY[1];
    dvdy /= gridSpacingXY[1];
    if mode in ['vort', 'vorticity']:
        return (dudy - dvdx)
    return dudx,dudy,dvdx,dvdy

def CheckNormalizedMedian(val, arrIN, thr, noiseFloor=0.05):
    roiMedian = np.median(arrIN)
    resid = np.abs(arrIN - roiMedian)
    residMedian= np.median(resid) + noiseFloor
    diff = np.abs(val - roiMedian)
    if residMedian > 0:
        diff = diff / residMedian #
        if diff > thr:
            return False
    return True # value is OK

def NormalizedMedianFilter(imgIN, thr=3):
    nr,nc = imgIN.shape
    mask = np.zeros_like(imgIN)
    noiseFloor = 0.05
    # crude/slow approach
    cnt=0
    for r in range(1,nr-1):
        for c in range(1,nc-1):
            roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
            roi = np.concatenate([roi[0:4], roi[5:9]]) # removes central value
            if not CheckNormalizedMedian(imgIN[r,c], roi, thr, noiseFloor=noiseFloor):
                cnt +=1
                mask[r,c] += 1
    # check top edge
    r = 1
    for c in range(1,nc-1):
        roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
        roi = np.concatenate([roi[0:1], roi[2:9]]) # removes central value
        if not CheckNormalizedMedian(imgIN[r-1,c], roi, thr, noiseFloor=noiseFloor):
            cnt +=1
            mask[r-1,c] += 1
    # check lower edge
    r = nr -2
    for c in range(1,nc-1):
        roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
        roi = np.concatenate([roi[0:7], roi[8:9]]) # removes central value
        if not CheckNormalizedMedian(imgIN[r+1,c], roi, thr, noiseFloor=noiseFloor):
            cnt +=1
            mask[r+1,c] += 1
    # check left edge
    c = 1
    for r in range(1,nr-1):
        roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
        roi = np.concatenate([roi[0:3], roi[4:9]]) # removes central value
        if not CheckNormalizedMedian(imgIN[r,c-1], roi, thr, noiseFloor=noiseFloor):
            cnt +=1
            mask[r,c-1] += 1
    # check right edge
    c = nc-2
    for r in range(1,nr-1):
        roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
        roi = np.concatenate([roi[0:5], roi[6:9]]) # removes central value
        if not CheckNormalizedMedian(imgIN[r,c+1], roi, thr, noiseFloor=noiseFloor):
            cnt +=1
            mask[r,c+1] += 1
    # check upper left corner
    r = 1
    c = 1
    roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
    roi = roi[1:9] # removes central value
    if not CheckNormalizedMedian(imgIN[r-1,c-1], roi, thr, noiseFloor=noiseFloor):
        cnt +=1
        mask[r-1,c-1] += 1
    # check upper right corner
    r = 1
    c = nc-2
    roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
    roi = np.concatenate([roi[0:2], roi[3:9]]) # removes central value
    if not CheckNormalizedMedian(imgIN[r-1,c+1], roi, thr, noiseFloor=noiseFloor):
        cnt +=1
        mask[r-1,c+1] += 1
    # check lower left corner
    r = nr-2
    c = 1
    roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
    roi = np.concatenate([roi[0:6], roi[7:9]]) # removes central value
    if not CheckNormalizedMedian(imgIN[r+1,c-1], roi, thr, noiseFloor=noiseFloor):
        cnt +=1
        mask[r+1,c-1] += 1
    # check lower right corner
    r = nr-2
    c = nc-2
    roi = imgIN[r-1:r+2,c-1:c+2].flatten() 
    roi = roi[0:8] # removes central value
    if not CheckNormalizedMedian(imgIN[r+1,c+1], roi, thr, noiseFloor=noiseFloor):
        cnt +=1
        mask[r+1,c+1] += 1
    # r = nr -1
    # for c in range(1,nc-1):
    #     roi = imgIN[r-1:r+1,c-1:c+2].flatten() 
    #     roi = np.concatenate([roi[0:4], roi[5:6]]) # removes central value
    #     if not checkNormalizedMedian(imgIN[r,c], roi, thr, noiseFloor=noiseFloor):
    #         cnt +=1
    #         mask[r,c] += 1
    print('detected %d possible outliers' % (cnt))
    return mask
    

def InterpolateOutliers(arrIN, maskIN):
    arrOUT = arrIN.copy()
    mask = maskIN.copy()
    mask[np.where(maskIN>0)] = np.nan
    x, y = np.indices(arrIN.shape)
    arrOUT[np.isnan(mask)] = griddata(
        (x[~np.isnan(mask)], y[~np.isnan(mask)]), # points we know
        arrIN[~np.isnan(mask)],                    # values we know
        (x[np.isnan(mask)], y[np.isnan(mask)]),   # points to interpolate
        method='linear',    # Method of interpolation: ‘linear’, ‘nearest’, ‘cubic’ 
        )
    return arrOUT

def MedianFilter(vxIN,vyIN):
    mask = np.zeros_like(vxIN)
    maskVx = NormalizedMedianFilter(vxIN)
    maskVy = NormalizedMedianFilter(vyIN)
    mask[np.where(maskVx == 1)] += 1
    mask[np.where(maskVy == 1)] += 1
    vx = InterpolateOutliers(vxIN, mask)
    vy = InterpolateOutliers(vyIN, mask)
    return vx, vy, mask

def MagnitudeFilter(vxIN,vyIN, thr):
    mag = np.sqrt(vxIN*vxIN + vyIN*vyIN)
    mask = np.zeros_like(vxIN)
    mask[np.where(mag > thr)] = np.nan
    vxOUT = vxIN.copy()
    vyOUT = vyIN.copy()
    x, y = np.indices(vxIN.shape)
    vxOUT[np.isnan(mask)] = griddata(
        (x[~np.isnan(mask)], y[~np.isnan(mask)]), # points we know
        vxIN[~np.isnan(mask)],                    # values we know
        (x[np.isnan(mask)], y[np.isnan(mask)]),   # points to interpolate
        method='linear',    # Method of interpolation: ‘linear’, ‘nearest’, ‘cubic’ 
        )
    vyOUT[np.isnan(mask)] = griddata(
        (x[~np.isnan(mask)], y[~np.isnan(mask)]), # points we know
        vyIN[~np.isnan(mask)],                    # values we know
        (x[np.isnan(mask)], y[np.isnan(mask)]),   # points to interpolate
        method='linear',    # Method of interpolation: ‘linear’, ‘nearest’, ‘cubic’ 
        )
    return vxOUT,vyOUT

def DetectOutliers(vxIN,vyIN, mag=6, median=3):
    mask = np.zeros_like(vxIN)
    # check magnitude
    vmag = np.sqrt(vxIN*vxIN + vyIN*vyIN)
    if mag > 0:
        mask[np.where(vmag > mag)] += 1
    if median > 1:
        maskVx = NormalizedMedianFilter(vxIN, thr=median)
        maskVy = NormalizedMedianFilter(vyIN, thr=median)
        maskMag = NormalizedMedianFilter(vmag, thr=median)
        sumMask = maskVx + maskVy + maskMag
        mask[np.where(sumMask>1)] += 1
    return mask
