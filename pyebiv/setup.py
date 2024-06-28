# Available at setup time due to pyproject.toml
from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup
import sysconfig

__version__ = "0.2"

# used to enable PYBIND-specific code where necessary
extra_compile_args = [] #sysconfig.get_config_var('CFLAGS').split()
extra_compile_args.append("-DPYBIND11") 

# The main interface is through Pybind11Extension.
# * You can add cxx_std=11/14/17, and then build_ext can be removed.
# * You can set include_pybind11=false to add the include directory yourself,
#   say from a submodule.
#

ext_modules = [
    Pybind11Extension(
        "pyebiv",
        [
        "src/ebi_events.cpp",
        "src/ebi_image.cpp",
        "src/ebi_utils.cpp",
        "pyebiv/pyebiv.cpp",
        "pyebiv/pyebiv_pybind.cpp"
        ],
        # Example: passing in the version to the compiled code
        define_macros=[("VERSION_INFO", __version__)],
        # make the headers visible
        include_dirs=["./include","./pyebiv"],
        #extra_compile_args=extra_compile_args,
    ),
]

setup(
    name="pyebiv",
    version=__version__,
    author="cewdlr",
    url="https://github.com/ebiv/",
    description="EBIV interface using pybind11",
    long_description="",
    ext_modules=ext_modules,
    #extras_require={"test": "pytest"},
    # Currently, build_ext only provides an optional "highest supported C++
    # level" feature, but in the future it may provide more features.
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.9",
)
