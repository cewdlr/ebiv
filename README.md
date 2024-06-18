# EBIV - Event-based image velocimetry
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Python and C++ code implementations for the analysis of event-camera recordings obtained from fluid flows.


Input: raw event data represented as time-surfaces

![RawEvents](https://github.com/cewdlr/ebiv/blob/main/images/wallflow4_dense_3_0010.png)

Output: Recovered velocity field

![VelocityData](https://github.com/cewdlr/ebiv/blob/main/images/wallflow4_dense_3_corr_0010.png)

## Update
June 2024: 
- added source code for pyEBIV Python interface
- sample raw data of water jet recorded with pulsed-EBIV including Python script showing pulsed event-data handling

## Publications

[Associated publications] 
- "Event-based imaging velocimetry: an assessment of event-based cameras for the measurement of fluid flows"\
(Experiments in Fluids 63, Article number 101 (2022), DOI 10.1007/s00348-022-03441-6)\
[Open access] https://doi.org/10.1007/s00348-022-03441-6 

- "Event-based imaging velocimetry using pulsed illumination"\
(Experiments in Fluids 64, Article number 98 (2023), DOI 10.1007/s00348-023-03641-8)\
[Open access] https://doi.org/10.1007/s00348-023-03641-8 

```
@article{EBIV:2022,
    title = {Event-based imaging velocimetry: an assessment of event-based cameras for the measurement of fluid flows},
    author = {Willert, Christian and Klinner, Joachim},
    doi = {10.1007/s00348-022-03441-6},
    url = {https://doi.org/10.1007/s00348-022-03441-6},
    journal = {Exp. Fluids},
    volume = {63},
    pages = {101},
    year = {2022},
}

@article{PulsedEBIV:2023,
    title={Event-based imaging velocimetry using pulsed illumination},
    author = {Christian Willert},
    doi = {10.1007/s00348-023-03641-8},
    url = {https://doi.org/10.1007/s00348-023-03641-8},
    journal = {Exp. Fluids},
    volume={64},
    pages={98},
    year = {2023},	
}

```



## Further reading

https://github.com/uzh-rpg/event-based_vision_resources

## Acknowledgements

Part of the pyEBIV interface makes use of Prophesee's Metavision SDK, in particular portions of the raw-file decoder (metavision_evt3_raw_file_decoder.cpp), licensed under the Apache License, Version 2.0.

