from __future__ import annotations

import ctypes
import pathlib
import sys
from typing import Any

import numpy as np

_PACKAGE_DIR = pathlib.Path(__file__).resolve().parent


def _load_library() -> ctypes.CDLL:
    if sys.platform == "win32":
        candidates = sorted(_PACKAGE_DIR.glob("_flow_field_texture_builder_c*.dll"))
        if hasattr(sys, "add_dll_directory"):
            sys.add_dll_directory(str(_PACKAGE_DIR))
    elif sys.platform == "darwin":
        candidates = sorted(_PACKAGE_DIR.glob("lib_flow_field_texture_builder_c*.dylib"))
    else:
        candidates = sorted(_PACKAGE_DIR.glob("lib_flow_field_texture_builder_c*.so"))

    if not candidates:
        raise ImportError("native flow-field texture builder library was not found")
    return ctypes.CDLL(str(candidates[0]))


_LIB = _load_library()
_LIB.ffb_last_error.argtypes = []
_LIB.ffb_last_error.restype = ctypes.c_char_p
_LIB.ffb_create.argtypes = [ctypes.c_char_p]
_LIB.ffb_create.restype = ctypes.c_void_p
_LIB.ffb_destroy.argtypes = [ctypes.c_void_p]
_LIB.ffb_destroy.restype = None
_LIB.ffb_set_output_directory.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_LIB.ffb_set_output_directory.restype = ctypes.c_int
_LIB.ffb_set_texture_size.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ffb_set_texture_size.restype = ctypes.c_int
_LIB.ffb_set_projection_texture_size.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_LIB.ffb_set_projection_texture_size.restype = ctypes.c_int
_LIB.ffb_set_quikgrid_options.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_float,
    ctypes.c_long,
]
_LIB.ffb_set_quikgrid_options.restype = ctypes.c_int
_LIB.ffb_set_domain_from_arrays.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double),
    ctypes.c_size_t,
    ctypes.c_int,
]
_LIB.ffb_set_domain_from_arrays.restype = ctypes.c_int
_LIB.ffb_set_domain_from_vector.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double),
    ctypes.c_size_t,
    ctypes.c_char_p,
    ctypes.c_int,
]
_LIB.ffb_set_domain_from_vector.restype = ctypes.c_int
_LIB.ffb_build_uv_texture.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float),
    ctypes.c_size_t,
    ctypes.c_char_p,
]
_LIB.ffb_build_uv_texture.restype = ctypes.c_int
_LIB.ffb_build_projection_texture.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
]
_LIB.ffb_build_projection_texture.restype = ctypes.c_int
_LIB.ffb_texture_width.argtypes = [ctypes.c_void_p]
_LIB.ffb_texture_width.restype = ctypes.c_int
_LIB.ffb_texture_height.argtypes = [ctypes.c_void_p]
_LIB.ffb_texture_height.restype = ctypes.c_int
_LIB.ffb_projection_texture_width.argtypes = [ctypes.c_void_p]
_LIB.ffb_projection_texture_width.restype = ctypes.c_int
_LIB.ffb_projection_texture_height.argtypes = [ctypes.c_void_p]
_LIB.ffb_projection_texture_height.restype = ctypes.c_int
_LIB.ffb_domain_extent.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double)]
_LIB.ffb_domain_extent.restype = ctypes.c_int
_LIB.ffb_projection_extent.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double)]
_LIB.ffb_projection_extent.restype = ctypes.c_int


def _bytes_path(path: str | pathlib.Path) -> bytes:
    return str(path).encode("utf-8")


def _check(status: int) -> None:
    if status == 0:
        return
    message = _LIB.ffb_last_error()
    raise RuntimeError(message.decode("utf-8") if message else "flow-field texture builder failed")


def _array_1d(values: Any, dtype: np.dtype[Any], name: str) -> np.ndarray[Any, np.dtype[Any]]:
    array = np.ascontiguousarray(values, dtype=dtype)
    if array.ndim != 1:
        raise ValueError(f"{name} must be one-dimensional")
    return array


def _validate_output_name(output_name: str) -> None:
    path = pathlib.PurePath(output_name)
    if (
        not output_name
        or "\\" in output_name
        or path.is_absolute()
        or len(path.parts) != 1
        or output_name in {".", ".."}
    ):
        raise ValueError("output_name must be a file stem")


class FlowFieldBuilder:
    def __init__(self, output_directory: str | pathlib.Path | None = None):
        self._output_directory = pathlib.Path(output_directory) if output_directory is not None else pathlib.Path()
        encoded = _bytes_path(self._output_directory) if output_directory is not None else None
        self._handle = _LIB.ffb_create(encoded)
        if not self._handle:
            _check(-1)

    def close(self) -> None:
        if self._handle:
            _LIB.ffb_destroy(self._handle)
            self._handle = None

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    def _require_handle(self) -> ctypes.c_void_p:
        if not self._handle:
            raise RuntimeError("builder is closed")
        return self._handle

    def set_output_directory(self, output_directory: str | pathlib.Path) -> None:
        self._output_directory = pathlib.Path(output_directory)
        _check(_LIB.ffb_set_output_directory(self._require_handle(), _bytes_path(self._output_directory)))

    def set_texture_size(self, width: int, height: int) -> None:
        _check(_LIB.ffb_set_texture_size(self._require_handle(), width, height))

    def set_projection_texture_size(self, width: int, height: int) -> None:
        _check(_LIB.ffb_set_projection_texture_size(self._require_handle(), width, height))

    def set_quikgrid_options(
        self,
        scan_ratio: int = 16,
        density_ratio: int = 150,
        edge_factor: int = 100,
        undefined_value: float = -99999.0,
        sample: int = 1,
    ) -> None:
        _check(
            _LIB.ffb_set_quikgrid_options(
                self._require_handle(),
                scan_ratio,
                density_ratio,
                edge_factor,
                ctypes.c_float(undefined_value),
                sample,
            )
        )

    def set_domain_from_arrays(self, xs: Any, ys: Any, source_epsg: int | None = None) -> None:
        xs_array, ys_array = self._domain_arrays(xs, ys)
        _check(
            _LIB.ffb_set_domain_from_arrays(
                self._require_handle(),
                xs_array.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                ys_array.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                xs_array.size,
                0 if source_epsg is None else source_epsg,
            )
        )

    def set_domain_from_vector(
        self,
        xs: Any,
        ys: Any,
        vector_path: str | pathlib.Path,
        source_epsg: int | None = None,
    ) -> None:
        xs_array, ys_array = self._domain_arrays(xs, ys)
        _check(
            _LIB.ffb_set_domain_from_vector(
                self._require_handle(),
                xs_array.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                ys_array.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                xs_array.size,
                _bytes_path(vector_path),
                0 if source_epsg is None else source_epsg,
            )
        )

    def build_uv_texture(self, us: Any, vs: Any, output_name: str) -> None:
        _validate_output_name(output_name)
        us_array = _array_1d(us, np.float32, "us")
        vs_array = _array_1d(vs, np.float32, "vs")
        if us_array.size != vs_array.size:
            raise ValueError("us and vs must have the same length")
        _check(
            _LIB.ffb_build_uv_texture(
                self._require_handle(),
                us_array.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                vs_array.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                us_array.size,
                output_name.encode("utf-8"),
            )
        )

    def build_projection_texture(self, target: str | int, output_name: str | None = None) -> None:
        target_name = str(target)
        output_name = target_name if output_name is None else output_name
        _validate_output_name(output_name)
        _check(
            _LIB.ffb_build_projection_texture(
                self._require_handle(),
                target_name.encode("utf-8"),
                output_name.encode("utf-8"),
            )
        )

    @property
    def texture_size(self) -> tuple[int, int]:
        handle = self._require_handle()
        return (_LIB.ffb_texture_width(handle), _LIB.ffb_texture_height(handle))

    @property
    def projection_texture_size(self) -> tuple[int, int]:
        handle = self._require_handle()
        return (_LIB.ffb_projection_texture_width(handle), _LIB.ffb_projection_texture_height(handle))

    @property
    def domain_extent(self) -> tuple[float, float, float, float]:
        output = (ctypes.c_double * 4)()
        _check(_LIB.ffb_domain_extent(self._require_handle(), output))
        return (output[0], output[1], output[2], output[3])

    @property
    def projection_extent(self) -> tuple[float, float, float, float]:
        output = (ctypes.c_double * 4)()
        _check(_LIB.ffb_projection_extent(self._require_handle(), output))
        return (output[0], output[1], output[2], output[3])

    def uv_texture_path(self, output_name: str) -> str:
        _validate_output_name(output_name)
        return str(self._output_directory / f"uv_{output_name}.png")

    def seed_texture_path(self, output_name: str) -> str:
        _validate_output_name(output_name)
        return str(self._output_directory / f"seed_{output_name}.png")

    def projection_texture_path(self, output_name: str) -> str:
        _validate_output_name(output_name)
        return str(self._output_directory / f"projection_{output_name}.png")

    @staticmethod
    def _domain_arrays(xs: Any, ys: Any) -> tuple[np.ndarray[Any, np.dtype[Any]], np.ndarray[Any, np.dtype[Any]]]:
        xs_array = _array_1d(xs, np.float64, "xs")
        ys_array = _array_1d(ys, np.float64, "ys")
        if xs_array.size != ys_array.size:
            raise ValueError("xs and ys must have the same length")
        return xs_array, ys_array


__all__ = ["FlowFieldBuilder"]
