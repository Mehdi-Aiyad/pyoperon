// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright 2019-2021 Heal Research

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>

#include <operon/core/dataset.hpp>
#include <utility>

#include "pyoperon/pyoperon.hpp"

namespace py = pybind11;

template<typename T>
auto MakeDataset(py::array_t<T> array) -> Operon::Dataset
{
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type.");

    // sanity check
    if (array.ndim() != 2) {
        throw std::runtime_error("The input array must have exactly two dimensions.\n");
    }

    // check if the array satisfies our data storage requirements (contiguous, column-major order)
    if (std::is_same_v<T, Operon::Scalar> && (array.flags() & py::array::f_style)) { // NOLINT
        auto ref = array.template cast<Eigen::Ref<Operon::Dataset::Matrix const>>();
        return Operon::Dataset(ref);
    }

#if defined(DEBUG)
    fmt::print(stderr, "operon warning: array does not satisfy contiguity or storage-order requirements. data will be copied.\n");
#endif
    auto mat = array.template cast<Operon::Dataset::Matrix>();
    return Operon::Dataset(std::move(mat));
}

template<typename T>
auto MakeDataset(std::vector<std::vector<T>> const& values) -> Operon::Dataset
{
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type.");

    auto rows = values[0].size();
    auto cols = values.size();

    Operon::Dataset::Matrix mat(rows, cols);

    for (size_t i = 0; i < values.size(); ++i) {
        mat.col(static_cast<int>(i)) = Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, 1, Eigen::ColMajor> const>(values[i].data(), rows).template cast<Operon::Scalar>();
    }
    return Operon::Dataset(std::move(mat));
}

auto MakeDataset(py::buffer buf) -> Operon::Dataset // NOLINT
{
    auto info = buf.request();

    if (info.ndim != 2) {
        throw std::runtime_error("The buffer must have two dimensions.\n");
    }

    if (info.format == py::format_descriptor<Operon::Scalar>::format()) {
        auto ref = buf.template cast<Eigen::Ref<Operon::Dataset::Matrix const>>();
        return Operon::Dataset(ref);
    }

#if defined(DEBUG)
    fmt::print(stderr, "operon warning: array does not satisfy contiguity or storage-order requirements. data will be copied.\n");
#endif
    auto mat = buf.template cast<Operon::Dataset::Matrix>();
    return Operon::Dataset(std::move(mat));
}


void InitDataset(py::module_ &m)
{
    // dataset
    py::class_<Operon::Dataset>(m, "Dataset")
        .def(py::init<std::string const&, bool>(), py::arg("filename"), py::arg("has_header"))
        .def(py::init<Operon::Dataset const&>())
        .def(py::init<std::vector<Operon::Variable> const&, const std::vector<std::vector<Operon::Scalar>>&>())
        .def(py::init([](py::array_t<float> array){ return MakeDataset(std::move(array)); }), py::arg("data").noconvert())
        .def(py::init([](py::array_t<double> array){ return MakeDataset(std::move(array)); }), py::arg("data").noconvert())
        .def(py::init([](std::vector<std::vector<float>> const& values) { return MakeDataset(values); }), py::arg("data").noconvert())
        .def(py::init([](std::vector<std::vector<double>> const& values) { return MakeDataset(values); }), py::arg("data").noconvert())
        .def(py::init([](py::buffer buf) { return MakeDataset(std::move(buf)); }), py::arg("data").noconvert())
        .def_property_readonly("Rows", &Operon::Dataset::Rows)
        .def_property_readonly("Cols", &Operon::Dataset::Cols)
        .def_property_readonly("Values", &Operon::Dataset::Values)
        .def_property("VariableNames", &Operon::Dataset::VariableNames, &Operon::Dataset::SetVariableNames)
        .def("GetValues", [](Operon::Dataset const& self, std::string const& name) { return MakeView(self.GetValues(name)); })
        .def("GetValues", [](Operon::Dataset const& self, Operon::Hash hash) { return MakeView(self.GetValues(hash)); })
        .def("GetValues", [](Operon::Dataset const& self, int index) { return MakeView(self.GetValues(index)); })
        .def("GetVariable", py::overload_cast<const std::string&>(&Operon::Dataset::GetVariable, py::const_))
        .def("GetVariable", py::overload_cast<Operon::Hash>(&Operon::Dataset::GetVariable, py::const_))
        .def_property_readonly("Variables", [](Operon::Dataset const& self) {
            auto vars = self.Variables();
            return std::vector<Operon::Variable>(vars.begin(), vars.end());
        })
        .def("Shuffle", &Operon::Dataset::Shuffle)
        .def("Normalize", &Operon::Dataset::Normalize)
        .def("Standardize", &Operon::Dataset::Standardize)
        ;
}
