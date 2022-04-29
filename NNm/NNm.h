/*

Copyright (c) 2020, Douglas Santry
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, is permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _NN_MATRIX__H__
#define _NN_MATRIX__H__

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include <data.h>
#include <NeuralM.h>
#include <sampling.h>

#include <stratum.h>
#include <RPROP.h>
#include <ADAM.h>

typedef stratum_t * (*Rule_t)(const int, const int);

template<typename T> class NNet_t
{
protected:

	int					n_steps;

	// morphology of the net
	int					n_Nin;
	int					n_Nout;
	int					n_levels;
	int					*n_width;		// array of lengths of n_nn
	stratum_t			**n_strata;

	int					n_Nweights;

	IEEE_t				n_halt;			// target loss
	IEEE_t				n_error;		// current loss
	bool				n_accuracy;		// halt training at 100% correct
	int					n_maxIterations;
	int					n_keepalive;	// how often to print status

	// Stochastic Gradient Descent Implementation
	bool					n_useSGD;		// SGD turned on
	IEEE_t					n_SGDn;			// % of batch to use
	NoReplacementSamples_t	*n_SGDsamples;	// permuted samples

	bool				n_normalize;
	IEEE_t				*n_normParams;
	IEEE_t				*n_arg;

	bool TrainWork (const DataSet_t * const);
	bool Step(const DataSet_t * const training);
	bool Halt (DataSet_t const * const);
	
public:

	/*
	 * levels is the number of layers, including the output.  width is an
	 * array specifying the width of each level.  e.g., { 1, 4, 1 }, is
	 * an SLP with a single input, 4 hidden and 1 output perceptron.
	 *
	 */
	NNet_t (const int * const width, 
			const int levels,
			Rule_t alloc) :
		n_steps (0),
		n_Nin (width[0]),
		n_Nout (width[levels - 1]),
		n_levels (levels - 1), // no state for input
		n_halt (1e-5),
		n_error (nan (NULL)),
		n_accuracy (false),
		n_maxIterations (5000),
		n_keepalive (100),
		n_useSGD (false),
		n_SGDn (nan(NULL)),
		n_SGDsamples (NULL),
		n_normalize (false),
		n_normParams (NULL),
		n_arg (NULL)
	{
		n_Nweights = 0;

		// width = # inputs, width 1, ..., # outputs

		for (int i = levels - 1; i > 0; --i)
			n_Nweights += width[i] * (width[i - 1] + 1); // + 1 for bias

		n_strata = new stratum_t * [n_levels];
		n_width = new int [n_levels];

		printf ("In\tOut\tTrainable\n");
		printf ("%d\t%d\t%d\n", n_Nin, n_Nout, n_Nweights);

		// start at 1, ignore inputs
		for (int i = 1; i <= n_levels; ++i)
		{
			n_width[i - 1] = width[i];
			n_strata[i - 1] = (*alloc)(width[i], width[i - 1]);
			n_strata[i - 1]->init (i < n_levels ? width[i + 1] : width[i]);
		}
	}

	~NNet_t (void)
	{
		for (int i = 0; i < n_levels; ++i)
			delete n_strata[i];

		delete [] n_strata;

		if (n_normParams)
			delete [] n_normParams;

		if (n_arg)
			delete [] n_arg;
	}

	void SetMaxIterations (int maxIterations)
	{
		n_maxIterations = maxIterations;
	}

	void SetHalt (IEEE_t mse)
	{
		n_halt = mse;
	}

	bool Train (const DataSet_t * const);
	bool Train (const DataSet_t * const, int); // used only when stand-alone

	int Steps (void) const
	{
		return n_steps;
	}

	void SetAccuracy (void)
	{
		n_accuracy = true;
	}

	void TurnOffAccuracy (void)
	{
		n_accuracy = false;
	}

	void SetSGD (IEEE_t percentage)
	{
		n_useSGD = true;
		n_SGDn = percentage;
	}

	void SetKeepAlive (const int modulus)
	{
		n_keepalive = modulus;
	}

	/*
	 * The next 4 call the specialization.  They are public as
	 * CNN components need to access them to integrate training.
	 *
	 */
	void Start (void);								// calls Cycle
	IEEE_t Compute (IEEE_t *);						// calls f ()
	inline IEEE_t Loss (void);						// calls Error
	IEEE_t ComputeDerivative (const TrainingRow_t);	// calls bprop

	// The below are public so these objects can be integrated
	void UpdateWeights (void)
	{
		for (int i = 0; i < n_levels; ++i)
			n_strata[i]->Strategy ();
	}

	bool ExposeGradient (NeuralM_t &);

	void SetNormalize (DataSet_t const * const S)
	{
		assert (S->Nin () == n_Nin);

		n_normalize = true;
		n_normParams = new IEEE_t [n_Nin * 2];
		n_arg = new IEEE_t [n_Nin];

		for (int i = 0; i < n_Nin; ++i)
		{
			n_normParams[i * 2] = S->Mean (i);
			n_normParams[i * 2 + 1] = S->StdDev (i);
		}
	}
};

#include <NNm.tcc>

#endif // header inclusion

