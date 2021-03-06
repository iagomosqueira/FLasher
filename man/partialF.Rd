% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/generics.R, R/methods.R
\name{partialF}
\alias{partialF}
\alias{partialF,FLBiols,FLFisheries-method}
\alias{partialF,FLBiol,FLFisheries-method}
\title{Calculation of fisheries partial fishing mortalities}
\usage{
partialF(object, fisheries, ...)

\S4method{partialF}{FLBiols,FLFisheries}(object, fisheries, biol = seq(length(object)), fcb = "missing")

\S4method{partialF}{FLBiol,FLFisheries}(object, fisheries, fcb = "missing")
}
\arguments{
\item{object}{The exploited population or populations, `FLBiol` or `FLBiols`.}

\item{fisheries}{The fisheries exploiting the resource, `FLFisheries`.}

\item{...}{Any extra argument.}

\item{biol}{Position of the biols or biols to do the calculation for.}

\item{fcb}{FCB matrix of the fishery-catch-biol relationships.}
}
\description{
Fishing mortalities at age for one of both stock (`FLBiol`) are partitioned
along the fisheries (`FLFisheries`) exploiting them.
}
