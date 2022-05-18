# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    generate Fig.10 of EBIV ExiF-Paper
    showing sum-of-correlation method on actual data
"""

from ebiv_utils import EBIDataSet,FFT_CrossCorr2D,FindCorrPeak2D
from ebiv_params import EBIV_Config

import numpy as np
import os
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from mpl_toolkits.axes_grid1 import make_axes_locatable
from plot_utils import SetTimesFont,PlacePlotLabel


useROI = 0      # plots small ROIs shown in Fig.8
savePlot = 0    # generate output

#%% Processing parameters
dataDir = '../sample_data/'
cfgFile = dataDir + 'ebiv_params.cfg'
cfg = EBIV_Config(cfgFile)

fnRAW = cfg.rawFile()

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)
if not os.path.exists(cfg.plotDir):
    os.makedirs(cfg.plotDir)

SetTimesFont()

szSample = [cfg.sampleW,cfg.sampleH]  # size of sampling area [W,H]
# sampling location near bottom wall
posSample = cfg.sampleLocation2
timeSample = [cfg.t_start, cfg.t_start+cfg.t_sample]
fnSuffix = '_t%.fms_tau%.fms_x%d_y%d' % \
    ((cfg.t_sample/1000), (cfg.t_sep/1000), posSample[0], posSample[1])

#%% Load events from file and compute variance map
print("Loading events from: " + fnRAW )
ev = EBIDataSet(fnRAW)

#%% get samples
s = ev.sampleEvents(posSample, szSample, timeSample, polarity=cfg.polarity)
timeSample1 = [timeSample[0]-cfg.t_sep//2, timeSample[1]-cfg.t_sep//2]
timeSample2 = [timeSample[0]+cfg.t_sep//2, timeSample[1]+cfg.t_sep//2]
s1 = ev.sampleEvents(posSample, szSample, timeSample1, polarity=cfg.polarity)
s2 = ev.sampleEvents(posSample, szSample, timeSample2, polarity=cfg.polarity)

# rearrange sampled events into NT-slices
vol1 = s1.asVolume(cfg.NT)
vol2 = s2.asVolume(cfg.NT)

# compute cross-correlations for at NT slice pairs
corr = FFT_CrossCorr2D(vol1,vol2)
# perform sum-of-correlations
corr2D = corr.sum(axis=0)
corr_dx,corr_dy,maxCorr = FindCorrPeak2D(corr2D)
# pixel velocity is obtained by figuring in the time-separation of the samples
corr_vx = (corr_dx/cfg.t_sep)*1000
corr_vy = (corr_dy/cfg.t_sep)*1000
print('Shift dx:%.4f dy:%.4f pixel --> vx:%.4f vy:%.4f pixel/ms (corr: %.4f)' \
      %(corr_dx, -corr_dy, corr_vx, -corr_vy, maxCorr))

#%% plot correlation maps abnd samples ----------------------
sumImg1 = np.flipud(s1.renderSample())
sumZeros1 = np.where(sumImg1 == 0)
sumImg2 = np.flipud(s2.renderSample())
sumZeros2 = np.where(sumImg2 == 0)
#print('range of sum-image: %g,%g' %(sumImg1.min(), sumImg1.max()))
nr,nc = sumImg1.shape
sumImg1[sumZeros1] = np.nan
sumImg2[sumZeros2] = np.nan

szTitle = 14
cmapCorr = 'hot'
cmap2 = 'bwr'
bgcolor = '#CCCCCC'

fig, [[ax0, ax1, ax2],[ax3, ax4, ax5]] = plt.subplots(2,3, sharex=False, sharey=False, 
                                                      figsize=(8,5))
for ax in [ax0, ax1, ax2]:
    ax.set_facecolor(bgcolor)
extent=[-nc/2,nc/2,-nr/2,nr/2]

imgPos0 = ax0.imshow((sumImg1-cfg.t_sample/2)/1000, 
                     cmap=cmap2, interpolation='nearest', 
                     extent=extent,
                     origin='lower',
                     vmin=-cfg.t_sample/(2*1000),
                     vmax=cfg.t_sample/(2*1000))
imgPos1 = ax1.imshow((sumImg2-cfg.t_sample/2)/1000, 
                     cmap=cmap2, interpolation='nearest', 
                     extent=extent,
                     origin='lower',
                     vmin=-cfg.t_sample/(2*1000),
                     vmax=cfg.t_sample/(2*1000))
# add vector
corr_dy = -corr_dy  # origin at top of image!
xc = -corr_dx/2
yc = corr_dy/2
arrow = mpatches.FancyArrowPatch((xc, yc), (xc+corr_dx, yc+corr_dy), ec='k', fc='k',
                                 mutation_scale=15, mutation_aspect=1, )
ax0.add_patch(arrow)

# show sum-of-correlation map
# correlation is larger than sample
nc2 = nc*2-1
nr2 = nr*2-1
r1 = nr//2-1
c1 = nc//2-1
corr2D_roi = corr2D[r1:r1+nr, c1:c1+nc]
imgPos2 = ax2.imshow(corr2D_roi, cmap=cmapCorr, interpolation='nearest', 
                     origin='upper',
                     vmin = 0,
                     extent=extent, )#vmin=1, vmax=20)
ax2.axhline(0, color="white", ls='--', lw=0.5)
ax2.axvline(0, color="white", ls='--', lw=0.5)

t_range = [-cfg.t_sample/2, cfg.t_sample/2]
T1 = [t_range[0]-cfg.t_sep/2, t_range[1]-cfg.t_sep/2]
T2 = [t_range[0]+cfg.t_sep/2, t_range[1]+cfg.t_sep/2]
Tcorr = [-(cfg.t_sample)/2,(cfg.t_sample)/2]
aspect=(nc*1000)/(cfg.t_sample)
ax3.axhline(0, color="red", ls='--', lw=0.5)
ax4.axhline(0, color="red", ls='--', lw=0.5)
vol1_sum = vol1.sum(axis=1)
vol2_sum = vol2.sum(axis=1)
vol1_sum[np.where(vol1_sum>1)] =1
vol2_sum[np.where(vol2_sum>1)] =1
# show X-time projection No.1
imgPos3 = ax3.imshow(vol1_sum*10, 
                     cmap='gray_r', interpolation='nearest', 
                     extent=[-nc/2,nc/2,T1[0]/1000,T1[1]/1000],
                     origin='lower',
                     aspect=aspect,
                     vmax = 10
                     ) 
# show X-time projection No.2 including No.1 in dimmed gray
imgPos4 = ax4.imshow(vol2_sum*10 + vol1_sum, 
                     cmap='gray_r', interpolation='nearest', 
                     extent=[-nc/2,nc/2,T2[0]/1000,T2[1]/1000],
                     origin='lower',
                     aspect=aspect,
                     vmax = 10
                     ) 

# show X-time projection of correlation NT planes
nz,ny,nx = corr.shape
aspect=(nc*1000)/(cfg.t_sample)
nc2 = (nc*2)+1
corrProj = corr.sum(axis=1)[:,c1:c1+nc]
imgPos5 = ax5.imshow(corrProj, 
                     cmap=cmapCorr, interpolation='nearest', 
                     origin='lower',
                     vmin = 0,
                     aspect=aspect,
                     extent=[-nc/2,nc/2,Tcorr[0]/1000,Tcorr[1]/1000], )
ax5.axvline(0, color="white", ls='--', lw=0.5)

divider2 = make_axes_locatable(ax2)
cax2 = divider2.append_axes("right", size="5%", pad=0.15)
cbar2 = plt.colorbar(imgPos2, cax=cax2, orientation='vertical') 
cbar2.set_label(r'Corr. Coeff.', fontsize=szTitle)

ax0.set_title(r'Sample 1, $T_0 - \tau/2$', fontsize=szTitle)
ax1.set_title(r'Sample 2, $T_0 + \tau/2$', fontsize=szTitle)
ax2.set_title('Cross-correlation', fontsize=szTitle)
for ax in [ax0,ax1,ax2]:
    ax.set_xlim(-nc/2,nc/2)
    ax.set_ylim(-nr/2,nr/2)
for ax in [ax3,ax4,ax5]:
    ax.set_xlabel('X [pixel]', fontsize=szTitle)
ax0.set_ylabel('Y [pixel]', fontsize=szTitle)
ax3.set_ylabel('Time [ms]', fontsize=szTitle)
ax1.set(yticklabels=[])  

PlacePlotLabel(ax0, [-0.4,1.1], "a)")
PlacePlotLabel(ax1, [-0.3,1.1], "b)")
PlacePlotLabel(ax2, [-0.4,1.15], "c)")
PlacePlotLabel(ax3, [-0.4,1.05], "d)")
PlacePlotLabel(ax4, [-0.3,1.05], "e)")
PlacePlotLabel(ax5, [-0.4,1.05], "f)")

plt.tight_layout()
if savePlot:
    fnPlot = cfg.plotDir + 'samples_corr' + fnSuffix
    plt.savefig(fnPlot+'.png', dpi=200)
    plt.savefig(fnPlot+'.pdf', dpi=200)
    print('plot saved in: ' + fnPlot+'.png')

plt.show()


