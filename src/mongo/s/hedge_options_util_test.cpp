/**
 *    Copyright (C) 2020-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/client/read_preference.h"
#include "mongo/s/hedge_options_util.h"
#include "mongo/s/mongos_server_parameters_gen.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/duration.h"

namespace mongo {
namespace {

class HedgeOptionsUtilTestFixture : public unittest::Test {
protected:
    /**
     * Set the given server parameters.
     */
    void setParameters(const BSONObj& parameters) {
        const ServerParameter::Map& parameterMap = ServerParameterSet::getGlobal()->getMap();
        BSONObjIterator parameterIterator(parameters);

        while (parameterIterator.more()) {
            BSONElement parameter = parameterIterator.next();
            std::string parameterName = parameter.fieldName();

            ServerParameter::Map::const_iterator foundParameter = parameterMap.find(parameterName);
            uassertStatusOK(foundParameter->second->set(parameter));
        }
    }

    /**
     * Unset the given server parameters by setting them back to the default.
     */
    void unsetParameters(const BSONObj& parameters) {
        const ServerParameter::Map& parameterMap = ServerParameterSet::getGlobal()->getMap();
        BSONObjIterator parameterIterator(parameters);

        while (parameterIterator.more()) {
            BSONElement parameter = parameterIterator.next();
            std::string parameterName = parameter.fieldName();
            const auto defaultParameter = kDefaultParameters[parameterName];
            ASSERT_FALSE(defaultParameter.eoo());

            ServerParameter::Map::const_iterator foundParameter = parameterMap.find(parameterName);
            uassertStatusOK(foundParameter->second->set(defaultParameter));
        }
    }

    /**
     * Sets the given server parameters and creates ReadPreferenceSetting from 'rspObj' and extracts
     * HedgeOptions from it. If 'hedge' is true, asserts that the resulting HedgeOptions is not
     * empty, otherwise asserts that it is empty. Resets the server parameters to the defaults
     * before returning.
     */
    void checkHedgeOptions(const BSONObj& serverParameters,
                           const BSONObj& rspObj,
                           const bool hedge,
                           const int maxTimeMSForHedgedReads = kMaxTimeMSForHedgedReadsDefault) {
        setParameters(serverParameters);

        auto readPref = uassertStatusOK(ReadPreferenceSetting::fromInnerBSON(rspObj));
        auto hedgeOptions = extractHedgeOptions(readPref);

        if (hedge) {
            ASSERT_TRUE(hedgeOptions.has_value());
            ASSERT_EQ(hedgeOptions->maxTimeMSForHedgedReads, maxTimeMSForHedgedReads);
        } else {
            ASSERT_FALSE(hedgeOptions.has_value());
        }

        unsetParameters(serverParameters);
    }

    static inline const std::string kCollName = "testColl";

    static inline const std::string kReadHedgingModeFieldName = "readHedgingMode";
    static inline const std::string kMaxTimeMSForHedgedReadsFieldName = "maxTimeMSForHedgedReads";
    static inline const int kMaxTimeMSForHedgedReadsDefault = 10;

    static inline const BSONObj kDefaultParameters =
        BSON(kReadHedgingModeFieldName << "on" << kMaxTimeMSForHedgedReadsFieldName
                                       << kMaxTimeMSForHedgedReadsDefault);

private:
    ServiceContext::UniqueServiceContext _serviceCtx = ServiceContext::make();
    ServiceContext::UniqueClient _client = _serviceCtx->makeClient("RemoteCommandRequestTest");
};

TEST_F(HedgeOptionsUtilTestFixture, ExplicitOperationHedging) {
    const auto parameters = BSONObj();
    const auto rspObj = BSON("mode"
                             << "primaryPreferred"
                             << "hedge" << BSONObj());

    checkHedgeOptions(parameters, rspObj, true);
}

TEST_F(HedgeOptionsUtilTestFixture, ImplicitOperationHedging) {
    const auto parameters = BSONObj();
    const auto rspObj = BSON("mode"
                             << "nearest");

    checkHedgeOptions(parameters, rspObj, true);
}

TEST_F(HedgeOptionsUtilTestFixture, OperationHedgingDisabled) {
    const auto parameters = BSONObj();
    const auto rspObj = BSON("mode"
                             << "nearest"
                             << "hedge" << BSON("enabled" << false));

    checkHedgeOptions(parameters, rspObj, false);
}

TEST_F(HedgeOptionsUtilTestFixture, ReadHedgingModeOff) {
    const auto parameters = BSON(kReadHedgingModeFieldName << "off");
    const auto rspObj = BSON("mode"
                             << "nearest"
                             << "hedge" << BSONObj());

    checkHedgeOptions(parameters, rspObj, false);
}

TEST_F(HedgeOptionsUtilTestFixture, MaxTimeMSForHedgedReads) {
    const auto parameters =
        BSON(kReadHedgingModeFieldName << "on" << kMaxTimeMSForHedgedReadsFieldName << 100);

    const auto rspObj = BSON("mode"
                             << "nearest"
                             << "hedge" << BSONObj());

    checkHedgeOptions(parameters, rspObj, true, 100);
}

}  // namespace
}  // namespace mongo
