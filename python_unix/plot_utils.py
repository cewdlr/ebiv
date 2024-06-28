# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    Various utilites for plotting
"""

def SetFont(font):
    """
    Allows to set either Times (Serif) of non-serif font 
    """
    import matplotlib as mpl
    if  font == 'Times' \
    or font == 'Roman' \
    or font == 'serif':
        mathtext = 'stix'
        fontFamily = 'serif'
        fontSerif = "Times New Roman"
    else:
        mathtext = 'dejavusans' # ['dejavusans', 'dejavuserif', 'cm', 'stix', 'stixsans', 'custom']
        fontFamily = 'sans'
        fontSerif = "Calibri"
        
    print('Setting default font to: ' + font)
        
    params = {'legend.fontsize': 14,
              'axes.labelsize': 16,
              'axes.titlesize': 18,
              'xtick.labelsize' :14,
              'ytick.labelsize': 14,
              'grid.color': 'k',
              'grid.linestyle': ':',
              'grid.linewidth': 0.5,
              'mathtext.fontset' : mathtext,
              'mathtext.rm'      : fontFamily,
              'font.family'      : fontFamily,
              'font.serif'       : fontSerif, # "Times New Roman", # or "Times"          
             }
    mpl.rcParams.update(params)
    
def SetTimesFont():
    SetFont('Times')
def SetStandardFont():
    SetFont('sans')

# place label with relative axis coordinates
def PlacePlotLabel(ax, pos:[float,float], lbl:str, fontsize=16):
    [x1,x2] = ax.get_xlim()
    [y1,y2] = ax.get_ylim()
    ax.text(x1+pos[0]*(x2-x1), y1+pos[1]*(y2-y1), lbl, fontsize=fontsize)
