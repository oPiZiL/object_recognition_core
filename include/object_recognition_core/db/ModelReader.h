/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2009, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

/** This file describes a base class you should inherit from when creating your own cell that reads models from the
 * database. This base class simplifies the job of interpreting the parameters and triggering some model redownload
 * if the parameters/object ids changes
 */

#ifndef ork_db_model_reader
#define ork_db_model_reader

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/python.hpp>
#include <boost/python/stl_iterator.hpp>

#include <ecto/ecto.hpp>

#include <object_recognition_core/common/json_spirit/json_spirit_reader_template.h>
#include <object_recognition_core/common/types.h>
#include <object_recognition_core/db/model_utils.h>
#include <object_recognition_core/db/view.h>

namespace object_recognition_core
{
  namespace db
  {
    namespace bases
    {
      /** When creating your own cell to read models from the DB, inherit from that class and implement the virtual
       * functions.
       */
      struct ModelReaderBase
      {
        virtual
        ~ModelReaderBase()
        {
        }

        /** This is the most important function: it gets triggered whenever something changes DB-wise (parameters or
         * object ids). Typically, what you do in that function is load the models from the database and train some
         * classifier
         * @param db_documents
         */
        virtual void
        parameter_callback(const Documents & db_documents) = 0;

        void
        configure_impl()
        {
          // Make sure that whenever parameters related to the models or objects changes, the list of models is regenerated
          json_db_.set_callback(boost::bind(&ModelReaderBase::parameterCallbackJsonDb, this, _1));
          json_object_ids_.set_callback(boost::bind(&ModelReaderBase::parameterCallbackJsonObjectIds, this, _1));
          method_.set_callback(boost::bind(&ModelReaderBase::parameterCallbackMethod, this, _1));
          json_submethod_.set_callback(boost::bind(&ModelReaderBase::parameterCallbackJsonSubMethod, this, _1));
          json_object_ids_.dirty(true);
        }

        /** The db object used to make the queries */
        ObjectDbPtr db_;
        /** The list of object ids */
        std::vector<ObjectId> object_ids_;
        /** The DB documents relative to those object ids */
        Documents documents_;

        friend void
        declare_params_impl(ecto::tendrils& params);
      private:
        void
        parameterCallbackCommon()
        {
          if (!db_)
            return;

          // define the documents from the database
          documents_ = ModelDocuments(db_, object_ids_, *method_, *json_submethod_);

          parameter_callback(documents_);
        }

        void
        parameterCallbackJsonDb(const std::string& json_db)
        {
          *json_db_ = json_db;
          if (json_db_->empty())
            return;
          if (!db_)
            db_ = ObjectDbParameters(*json_db_).generateDb();

          parameterCallbackCommon();
        }

        void
        parameterCallbackJsonObjectIds(const std::string& json_object_ids)
        {
          // read the object ids from the JSON string
          object_ids_.clear();
          or_json::mArray array = or_json::mValue(json_object_ids).get_array();
          for (or_json::mArray::const_iterator iter = array.begin(), end = array.end(); iter != end; ++iter)
            object_ids_.push_back(iter->get_str());

          parameterCallbackCommon();
        }

        void
        parameterCallbackMethod(const std::string& method)
        {
          *method_ = method;
          parameterCallbackCommon();
        }

        void
        parameterCallbackJsonSubMethod(const std::string& json_submethod)
        {
          *json_submethod_ = json_submethod;
          parameterCallbackCommon();
        }

        /** The method used to compute the models */
        ecto::spore<std::string> method_;
        /** The sub-method used to compute the models */
        ecto::spore<std::string> json_submethod_;
        /** The DB parameter stored as a JSON string */
        ecto::spore<std::string> json_db_;
        /** The DB documents for the models stored as a JSON string*/
        ecto::spore<std::string> json_object_ids_;
      };

      void
      declare_params_impl(ecto::tendrils& params)
      {
        params.declare(&ModelReaderBase::json_db_, "json_db", "The DB configuration parameters as a JSON string").required();
        params.declare(&ModelReaderBase::json_object_ids_, "json_object_ids",
                       "A set of object ids as a JSON string: '[\"erwere\"]'", "all");
        params.declare(&ModelReaderBase::method_, "method", "The method the models were computed with").required();
        params.declare(&ModelReaderBase::json_submethod_, "json_submethod", "The submethod used to compute the models.",
                       "");
      }
    }
  }
}

#endif
