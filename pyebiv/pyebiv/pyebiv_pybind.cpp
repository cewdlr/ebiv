//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // for std::vector
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>

#include "pyebiv.h"
#include <iostream> // for cerr

namespace py = pybind11;


PYBIND11_MAKE_OPAQUE(std::vector<int32_t>);

PYBIND11_MODULE(pyebiv, m) {
    m.doc() = "pyEBIV plugin"; // module docstring

    PYBIND11_NUMPY_DTYPE(EBI::Event, t,x,y,p); 

    // define all classes
    py::class_<EBIV>(m, "EBIV") // ClassInterface>(m, "EBIV")
            .def(py::init<>())  // constructor
            .def(py::init<std::string const&>()) // constructor
            //.def_readwrite("aPublicMember", &EBIV::aPublicMember)
            .def("loadRaw", &EBIV::loadRaw)
            .def("save", &EBIV::save)
            .def("setDebugLevel", &EBIV::setDebugLevel)
            .def("width", &EBIV::width)
            .def("height", &EBIV::height)
            .def("eventCount", &EBIV::eventCount)
            .def("timeStamp", &EBIV::timeStamp)
            .def("meanPulseHistogram", &EBIV::meanPulseHistogram)
            .def("estimatePulseOffsetTime", &EBIV::estimatePulseOffsetTime)
            .def("pseudoImage", &EBIV::pseudoImage)
            .def("events", &EBIV::events)
            .def("sensorSize", &EBIV::sensorSize)
            .def("x", &EBIV::x)
            .def("y", &EBIV::y)
            .def("p", &EBIV::p)
            .def("time", &EBIV::time)           
            ;

    // for some odd reason this only needs to be specified once for the four calls
    py::bind_vector<std::vector<int32_t>>(m, "x");
    py::bind_vector<std::vector<float>>(m, "pseudoImage");
    py::bind_vector<std::vector<double>>(m, "meanPulseHistorgram");
    
    // define any standalone functions
    //m.def("StandAloneFunction", &StandAloneFunction);

}
