// Copyright (C) 2015 by Félix Darricau, AIST, CNRS, EPITA
//                       Grégoire Duchemin, AIST, CNRS, EPITA
//
// This file is part of the roboptim.
//
// roboptim is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// roboptim is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with roboptim.  If not, see <http://www.gnu.org/licenses/>.

#ifndef ROBOPTIM_CORE_MANIFOLD_MAP_DECORATOR_SUM_ON_MANIFOLD_HXX
# define ROBOPTIM_CORE_MANIFOLD_MAP_DECORATOR_SUM_ON_MANIFOLD_HXX

namespace roboptim
{

  template<typename T>
  SumOnManifold<T>::SumOnManifold(std::vector<std::shared_ptr<FunctionOnManifold<T> > >& functions,
		std::vector<double> weights,
		size_type inputSize, size_type outputSize, std::string name)
    : GenericTwiceDifferentiableFunction<T>(inputSize, outputSize, name),
      functions_(functions),
      weights_(weights),
      resultBuffer(outputSize),
      jacobianBuffer(outputSize, inputSize),
      hessianBuffer(inputSize, inputSize)
  {
    flags_ = functions_[0]->getFlags();

    for (auto func : functions_)
      {
	flags_ &= func->getFlags();
      }
  }

  template<typename T>
  void SumOnManifold<T>::impl_compute (result_ref result, const_argument_ref x)
    const
  {
    result.setZero();
    size_t i = 0;
    for (auto function : functions_)
      {
	resultBuffer.setZero();
	(*function)(resultBuffer, x);
	result += weights_[i++] * resultBuffer;
      }
  }

  template<typename T>
  void SumOnManifold<T>::impl_gradient (gradient_ref,
		      const_argument_ref,
		      size_type)
    const
  {
    std::cerr << "UNIMPLEMENTED GRADIENT IN SUM ON MANIFOLD" << std::endl;
  }

  template<typename T>
  void SumOnManifold<T>::impl_jacobian (jacobian_ref jacobian,
		      const_argument_ref arg)
    const
  {
    jacobian.setZero();
    size_t i = 0;
    for (auto function : functions_)
      {
	jacobianBuffer.setZero();
	function->jacobian(jacobianBuffer, arg);
	jacobian += weights_[i++] * jacobianBuffer;
      }
  }

  template<typename T>
  void SumOnManifold<T>::impl_hessian(hessian_ref hessian,
		    const_argument_ref arg,
		    size_type functionId)
    const
  {
    hessian.setZero();
    size_t i = 0;
    for (auto function : functions_)
      {
	hessianBuffer.setZero();
	function->hessian(hessianBuffer, arg, functionId);
	hessian += weights_[i++] * hessianBuffer;
      }
  }

  template<typename T>
  typename SumOnManifold<T>::flag_t SumOnManifold<T>::getFlags() const
  {
    return flags_;
  }


  // ---- //

  template<typename T>
  template<typename U, typename V>
  void AdderOnManifold<T>::add(double weight, DescriptiveWrapper<U, V>& descWrap, mnf::Manifold& instanceManifold, std::vector<const mnf::Manifold*>& restricted, std::vector<std::pair<long, long>>& restrictions)
  {
    descWrap_storage_t lambda =
      [&descWrap, this, &instanceManifold, restricted, restrictions]
      (const mnf::Manifold& globMani)
      {
	return std::shared_ptr<FunctionOnManifold<T>>
	(new WrapperOnManifold<T>
	 (descWrap, globMani, instanceManifold, restricted, restrictions)
	 );
      };

    functionsToSum_.push_back(lambda);
    weights_.push_back(weight);
    merger_.addManifold(instanceManifold);
  }

  template<typename T>
  template<typename U, typename V>
  void AdderOnManifold<T>::add(DescriptiveWrapper<U, V>& descWrap, mnf::Manifold& instanceManifold, std::vector<const mnf::Manifold*>& restricted, std::vector<std::pair<long, long>>& restrictions)
  {
    add(1.0, descWrap, instanceManifold, restricted, restrictions);
  }

  template<typename T>
  template<typename U, typename V>
  void AdderOnManifold<T>::add(DescriptiveWrapper<U, V>& descWrap, mnf::Manifold& instanceManifold)
  {
    std::vector<const mnf::Manifold*> restricted;
    std::vector<std::pair<long, long>> restrictions;

    add(descWrap, instanceManifold, restricted, restrictions);
  }

  template<typename T>
  template<typename U, typename V>
  void AdderOnManifold<T>::add(double weight, DescriptiveWrapper<U, V>& descWrap, mnf::Manifold& instanceManifold)
  {
    std::vector<const mnf::Manifold*> restricted;
    std::vector<std::pair<long, long>> restrictions;

    add(weight, descWrap, instanceManifold, restricted, restrictions);
  }

  template<typename T>
  std::shared_ptr<FunctionOnManifold<T>> AdderOnManifold<T>::getFunction(const mnf::Manifold& globMani)
  {
    mnf::Manifold* sumManifold = merger_.getManifold();

    std::vector<std::shared_ptr<FunctionOnManifold<T> > > functions;
    std::string name;

    for (descWrap_storage_t& lambda : functionsToSum_)
      {
	functions.push_back(lambda(*sumManifold));

	if (functions.size() > 1 && functions.back()->outputSize() != functions[functions.size() - 2]->outputSize())
	  {
	    std::cerr << "ERROR: function "<< functions.back()->getName()
		      << " of outputSize " << functions.back()->outputSize()
		      << " cannot be added to a sum of functions of outputSize " << functions[functions.size() - 2]->outputSize()
		      << std::endl;

	    throw std::runtime_error("Invalid size when summing FunctionOnManifold");
	  }

	name += (name.size() > 0 ? " ":"") + functions.back()->getName();
      }

    typedef DescriptiveWrapper<SumOnManifold<T>, ManiDesc<>> descWrap_t;

    SumOnManifold<T>* sumFunction = new SumOnManifold<T>(functions,
							 weights_,
							 functions[0]->inputSize(),
							 functions[0]->outputSize(),
							 name);

    descWrap_t* descWrap = descWrap_t::makeUNCHECKEDDescriptiveWrapper(sumFunction, *sumManifold);

    return std::shared_ptr<FunctionOnManifold<T>>(new WrapperOnManifold<T>
						  (*descWrap, globMani, *sumManifold)
						  );

    delete descWrap;
  }

  template<typename T>
  void AdderOnManifold<T>::clear()
  {
    functionsToSum_.clear();
    weights_.clear();
    merger_.clear();
  }

  template<typename T>
  mnf::Manifold* AdderOnManifold<T>::getManifold()
  {
    return merger_.getManifold();
  }

  template<typename T>
  size_t AdderOnManifold<T>::numberOfFunctions()
  {
    return functionsToSum_.size();
  }

}

#endif //! ROBOPTIM_CORE_MANIFOLD_MAP_DECORATOR_SUM_ON_MANIFOLD_HXX