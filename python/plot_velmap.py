# -*- coding: utf-8 -*-
"""

@author: C.Willert (cewdlr)

Purpose:
    Plot vector map of recovered velocity field
"""

import numpy as np
import os
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable
from velmap_utils import ComputeGradients2D,DetectOutliers,InterpolateOutliers

import matplotlib.colors
def vector_to_rgb(val):
    if(val == 0): return matplotlib.colors.to_rgb('k')
    return matplotlib.colors.to_rgb('red')
def vectorColor(arrIN):
    return np.array(list(map(vector_to_rgb, arrIN.flatten() ))).reshape(arrIN.size,3)

#%% Plotting parameters
savePlot = 0    # generate output
contPlot = 1    # add scalar map of velocity magnitude or vorticity
plotVorticity = 0
showTitle = 1
doValidation = 1
evalModes = ['corr', 'motion', 'piv']
evalMode = evalModes[0]

vmin = 0
vmax = 4000
vecScale = 80
# todo: these values should be part of data set / maybe use parameter set/dict
[imgH,imgW] = [720,1280]
timeSample = [0,10000]
t_offset = 3000
timeStep = 5000
velResol = 0.25
sampleW = 40
sampleH = 40
piv_delay = 2000

dataDir = '../sample_data/'
plotDir = dataDir + 'plots/'
if evalMode in ['corr']:
    fnStub = 'veldata_t10ms_w40_corr_tau3ms'
    #fnStub = 'veldata_t10ms_w32_corr_tau3ms'
else:
    fnStub = 'veldata_t10ms_w40_motion_res0_25'

fnIN = dataDir + fnStub + '.npz'
if not os.path.isfile(fnIN):
    raise IOError('File not found: ' + fnIN)
if not os.path.exists(plotDir):
    os.makedirs(plotDir)
    
# load the data 
npfile = np.load(fnIN)
array_names = npfile.files
velData = npfile[array_names[0]]
npfile.close()

nt,ny,nx,nvals = velData.shape

ix = velData[:,:,:,0].reshape(nt,ny,nx)
iy = velData[:,:,:,1].reshape(nt,ny,nx)
vx = velData[:,:,:,2].reshape(nt,ny,nx)
vy = velData[:,:,:,3].reshape(nt,ny,nx)
imgVar = velData[:,:,:,4].reshape(nt,ny,nx)
imgEvCnt = velData[:,:,:,5].reshape(nt,ny,nx)

timeSteps = [0]
for t_idx in timeSteps:

    fnOUT = plotDir + fnStub

    imgVar2D = imgVar[t_idx,:,:].reshape(ny,nx)
    imgCnt2D = imgEvCnt[t_idx,:,:].reshape(ny,nx)
    vx2D = vx[t_idx,:,:].reshape(ny,nx)
    vy2D = vy[t_idx,:,:].reshape(ny,nx)
    mask = np.zeros_like(vx2D)
    if doValidation:
        mask = DetectOutliers(vx2D,vy2D, median=3.5, mag=0)
        vx2Dint = InterpolateOutliers(vx2D, mask)
        vy2Dint = InterpolateOutliers(vy2D, mask)
    else:
        vx2Dint = vx2D
        vy2Dint = vy2D
    
    if ix.ndim == 2:
        ix2D = ix
        iy2D = iy
    else:
        ix2D = ix[t_idx,:,:].reshape(ny,nx)
        iy2D = iy[t_idx,:,:].reshape(ny,nx)
    imgMag2D = np.sqrt(vx2Dint*vx2Dint + vy2Dint*vy2Dint)
    img = imgMag2D * 1000 # to get [pixel/s]
    #img[np.where(mask > 0)] = np.nan
    cmap = 'jet'
    cbarLabel = r'Velocity magnitude [pixel/s]'

    dx = np.abs(ix2D[0,1] - ix2D[0,0])
    dy = np.abs(iy2D[1,0] - iy2D[0,0])
    if plotVorticity:
        vortField = ComputeGradients2D(vx2Dint,vy2Dint, 'vorticity',
                                       gridSpacingXY=[dx,dy], bSmooth=True)
        vortField *= 1000 # convert from 1/ms to 1/s
        img = vortField
        cmap = 'bwr'
        vmin = -50
        vmax = 50
        cbarLabel = r'Vorticity [1/s]'
        
    #%% generate plot
    figsize=(8,5.)
    if contPlot: figsize=(9,5.)
    fig, ax1 = plt.subplots(1,1, figsize=figsize)# sharex=True, sharey=True)
    
    
    extent = [ix2D.min(),ix2D.max(), iy2D.min(), iy2D.max()]
    ax1.set_aspect(1)
    
    if contPlot:
        imgPos = ax1.imshow(img, cmap=cmap, extent=extent, aspect=1, 
                            vmin=vmin, vmax=vmax)
    invalid = np.where(mask > 0)
    vecColor = vectorColor(mask)
    Q1 = ax1.quiver(ix2D,iy2D, vx2Dint,vy2Dint, color=vecColor,
                cmap=cmap,
               units = 'width',
               scale_units = 'width',
               scale = vecScale,
               pivot='mid',
               )
    pad = 15
    ax1.set_xlim(-pad,imgW+pad)
    ax1.set_ylim(-pad,imgH+pad)
    s = 'Algorithm: Motion compensation - '
    s += '%g ms, %dx%d-sample, resol:%gpx/s, single pass' \
        % ((timeSample[1]-timeSample[0])/1000, sampleW,sampleH, velResol*1000)
    if evalMode == 'corr':
        s = 'Algorithm: Sum-of-correlation - '
        s += '%g ms (%g ms offset) %dx%d-sample, single pass' \
            % ((timeSample[1]-timeSample[0])/1000, t_offset/1000, sampleW,sampleH)
    elif evalMode == 'piv':
        s = 'Algorithm: Multi-frame PIV - '
        s += '4x %g ms, %dx%d-sample, 50%% overlap, multi-pass' \
            % ((piv_delay)/1000, sampleW,sampleH)
    if doValidation:
        s += ', validated'
    else:
        s += ', raw-data (no filter)'
    ax1.set_xlabel('X - [pixel]', fontsize=14)
    ax1.set_ylabel('Y - [pixel]', fontsize=14)
    if showTitle:
        ax1.set_title(s, fontsize=8)
        timeLabel = ('Time %.2f ms' % (t_idx * timeStep/1000))
        xlim = ax1.get_xlim()
        ylim = ax1.get_ylim()
        ax1.text(xlim[0]-0.1*(xlim[1]-xlim[0]), 
                 ylim[1]+0.02*(ylim[1]-ylim[0]), timeLabel, fontsize=16)
    
    if contPlot:
        divider = make_axes_locatable(ax1)
        cax = divider.append_axes("right", size="3%", pad=0.05)
        cbar = plt.colorbar(imgPos, cax=cax)  
        cbar.set_label(cbarLabel, fontsize=14)
        cbar.minorticks_on()
    
    plt.tight_layout()
    if savePlot:
        if not contPlot:
            fnOUT += '_vec'
        else:
            if(plotVorticity):
                fnOUT += '_vort'
        
        if timeSteps.size == 1:
            plt.savefig(fnOUT+'.pdf')
        else:
            fnOUT += ('_%04d' % t_idx)
        plt.savefig(fnOUT+'.png', dpi=200)
        print('plot stored in: ' + fnOUT + '.pdf/.png')
    plt.show()

