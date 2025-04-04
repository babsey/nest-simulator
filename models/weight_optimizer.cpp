/*
 *  weight_optimizer.cpp
 *
 *  This file is part of NEST.
 *
 *  Copyright (C) 2004 The NEST Initiative
 *
 *  NEST is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  NEST is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NEST.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "weight_optimizer.h"

// nestkernel
#include "exceptions.h"
#include "nest_names.h"

// sli
#include "dictutils.h"

namespace nest
{
WeightOptimizerCommonProperties::WeightOptimizerCommonProperties()
  : batch_size_( 1 )
  , eta_( 1e-4 )
  , eta_first_change_( 1e-4 )
  , eta_change_count_( 0 )
  , Wmin_( -100.0 )
  , Wmax_( 100.0 )
  , optimize_each_step_( true )
{
}

WeightOptimizerCommonProperties::WeightOptimizerCommonProperties( const WeightOptimizerCommonProperties& cp )
  : batch_size_( cp.batch_size_ )
  , eta_( cp.eta_ )
  , eta_first_change_( cp.eta_first_change_ )
  , eta_change_count_( cp.eta_change_count_ )
  , Wmin_( cp.Wmin_ )
  , Wmax_( cp.Wmax_ )
  , optimize_each_step_( cp.optimize_each_step_ )
{
}

void
WeightOptimizerCommonProperties::get_status( DictionaryDatum& d ) const
{
  def< std::string >( d, names::optimizer, get_name() );
  def< long >( d, names::batch_size, batch_size_ );
  def< double >( d, names::eta, eta_ );
  def< double >( d, names::Wmin, Wmin_ );
  def< double >( d, names::Wmax, Wmax_ );
  def< bool >( d, names::optimize_each_step, optimize_each_step_ );
}

void
WeightOptimizerCommonProperties::set_status( const DictionaryDatum& d )
{
  long new_batch_size = batch_size_;
  updateValue< long >( d, names::batch_size, new_batch_size );
  if ( new_batch_size <= 0 )
  {
    throw BadProperty( "Optimization batch_size > 0 required." );
  }
  batch_size_ = new_batch_size;

  double eta_new = eta_;
  updateValue< double >( d, names::eta, eta_new );
  if ( eta_new < 0 )
  {
    throw BadProperty( "Learning rate eta ≥ 0 required." );
  }

  if ( eta_new != eta_ )
  {
    if ( eta_change_count_ == 0 )
    {
      eta_first_change_ = eta_new;
    }
    eta_change_count_ += 1;
    eta_ = eta_new;
  }

  double new_Wmin = Wmin_;
  double new_Wmax = Wmax_;
  updateValue< double >( d, names::Wmin, new_Wmin );
  updateValue< double >( d, names::Wmax, new_Wmax );
  if ( new_Wmin > new_Wmax )
  {
    throw BadProperty( "Minimal weight Wmin ≤ maximal weight Wmax required." );
  }
  Wmin_ = new_Wmin;
  Wmax_ = new_Wmax;

  updateValue< bool >( d, names::optimize_each_step, optimize_each_step_ );
}

WeightOptimizer::WeightOptimizer()
  : sum_gradients_( 0.0 )
  , optimization_step_( 1 )
  , eta_current_( 1e-4 )
  , n_optimize_( 0 )
{
}

void
WeightOptimizer::get_status( DictionaryDatum& d ) const
{
}

void
WeightOptimizer::set_status( const DictionaryDatum& d )
{
}

double
WeightOptimizer::optimized_weight( const WeightOptimizerCommonProperties& cp,
  const size_t idx_current_update,
  const double gradient,
  double weight )
{
  if ( cp.eta_change_count_ != 0 and n_optimize_ == 0 )
  {
    eta_current_ = cp.eta_first_change_;
  }
  sum_gradients_ += gradient;

  if ( optimization_step_ == 0 )
  {
    optimization_step_ = idx_current_update;
  }

  const size_t current_optimization_step = 1 + idx_current_update / cp.batch_size_;
  if ( optimization_step_ < current_optimization_step )
  {
    sum_gradients_ /= cp.batch_size_;
    weight = std::max( cp.Wmin_, std::min( optimize_( cp, weight, current_optimization_step ), cp.Wmax_ ) );
    eta_current_ = cp.eta_;
    n_optimize_ += 1;
    optimization_step_ = current_optimization_step;
  }
  return weight;
}

WeightOptimizerCommonProperties*
WeightOptimizerCommonPropertiesGradientDescent::clone() const
{
  return new WeightOptimizerCommonPropertiesGradientDescent( *this );
}

WeightOptimizer*
WeightOptimizerCommonPropertiesGradientDescent::get_optimizer() const
{
  return new WeightOptimizerGradientDescent();
}

WeightOptimizerGradientDescent::WeightOptimizerGradientDescent()
  : WeightOptimizer()
{
}

double
WeightOptimizerGradientDescent::optimize_( const WeightOptimizerCommonProperties& cp, double weight, size_t )
{
  weight -= eta_current_ * sum_gradients_;
  sum_gradients_ = 0.0;
  return weight;
}

WeightOptimizerCommonPropertiesAdam::WeightOptimizerCommonPropertiesAdam()
  : WeightOptimizerCommonProperties()
  , beta_1_( 0.9 )
  , beta_2_( 0.999 )
  , epsilon_( 1e-7 )
{
}


WeightOptimizerCommonProperties*
WeightOptimizerCommonPropertiesAdam::clone() const
{
  return new WeightOptimizerCommonPropertiesAdam( *this );
}

WeightOptimizer*
WeightOptimizerCommonPropertiesAdam::get_optimizer() const
{
  return new WeightOptimizerAdam();
}

void
WeightOptimizerCommonPropertiesAdam::get_status( DictionaryDatum& d ) const
{
  WeightOptimizerCommonProperties::get_status( d );

  def< double >( d, names::beta_1, beta_1_ );
  def< double >( d, names::beta_2, beta_2_ );
  def< double >( d, names::epsilon, epsilon_ );
}

void
WeightOptimizerCommonPropertiesAdam::set_status( const DictionaryDatum& d )
{
  WeightOptimizerCommonProperties::set_status( d );

  updateValue< double >( d, names::beta_1, beta_1_ );
  updateValue< double >( d, names::beta_2, beta_2_ );
  updateValue< double >( d, names::epsilon, epsilon_ );

  if ( beta_1_ < 0.0 or 1.0 <= beta_1_ )
  {
    throw BadProperty( "For Adam optimizer, beta_1 from interval [0,1) required." );
  }

  if ( beta_2_ < 0.0 or 1.0 <= beta_2_ )
  {
    throw BadProperty( "For Adam optimizer, beta_2 from interval [0,1) required." );
  }

  if ( epsilon_ < 0.0 )
  {
    throw BadProperty( "For Adam optimizer, epsilon ≥ 0 required." );
  }
}

WeightOptimizerAdam::WeightOptimizerAdam()
  : WeightOptimizer()
  , m_( 0.0 )
  , v_( 0.0 )
  , beta_1_power_( 1.0 )
  , beta_2_power_( 1.0 )
{
}

void
WeightOptimizerAdam::get_status( DictionaryDatum& d ) const
{
  WeightOptimizer::get_status( d );
  def< double >( d, names::m, m_ );
  def< double >( d, names::v, v_ );
}

void
WeightOptimizerAdam::set_status( const DictionaryDatum& d )
{
  WeightOptimizer::set_status( d );
  updateValue< double >( d, names::m, m_ );
  updateValue< double >( d, names::v, v_ );
}


double
WeightOptimizerAdam::optimize_( const WeightOptimizerCommonProperties& cp,
  double weight,
  size_t current_optimization_step )
{
  const WeightOptimizerCommonPropertiesAdam& acp = dynamic_cast< const WeightOptimizerCommonPropertiesAdam& >( cp );

  for ( ; optimization_step_ < current_optimization_step; ++optimization_step_ )
  {
    beta_1_power_ *= acp.beta_1_;
    beta_2_power_ *= acp.beta_2_;

    const double alpha = eta_current_ * std::sqrt( 1.0 - beta_2_power_ ) / ( 1.0 - beta_1_power_ );

    m_ = acp.beta_1_ * m_ + ( 1.0 - acp.beta_1_ ) * sum_gradients_;
    v_ = acp.beta_2_ * v_ + ( 1.0 - acp.beta_2_ ) * sum_gradients_ * sum_gradients_;

    weight -= alpha * m_ / ( std::sqrt( v_ ) + acp.epsilon_ );

    // set gradients to zero for following iterations since more than
    // one cycle indicates past learning periods with vanishing gradients
    sum_gradients_ = 0.0; // reset for following iterations
  }

  return weight;
}

} // namespace nest
