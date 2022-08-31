// -*- C++ -*-
/*!
 * @file  PressDriver.h
 * @brief ModuleDescription
 * @date  $Date$
 *
 * $Id$
 */

#ifndef PRESSDRIVER_H
#define PRESSDRIVER_H

#include <rtm/idl/BasicDataTypeSkel.h>
#include <rtm/idl/ExtendedDataTypesSkel.h>
#include <rtm/idl/InterfaceDataTypesSkel.h>

 // Service implementation headers
 // <rtc-template block="service_impl_h">

 // </rtc-template>

 // Service Consumer stub headers
 // <rtc-template block="consumer_stub_h">
#include "BasicDataTypeStub.h"

// </rtc-template>

#include <rtm/Manager.h>
#include <rtm/DataFlowComponentBase.h>
#include <rtm/CorbaPort.h>
#include <rtm/DataInPort.h>
#include <rtm/DataOutPort.h>

/*!
 * @class PressDriver
 * @brief ModuleDescription
 *
 */
class PressDriver
	: public RTC::DataFlowComponentBase
{
public:
	/*!
	 * @brief constructor
	 * @param manager Maneger Object
	 */
	PressDriver(RTC::Manager* manager);

	/*!
	 * @brief destructor
	 */
	~PressDriver();

	// <rtc-template block="public_attribute">

	// </rtc-template>

	// <rtc-template block="public_operation">

	// </rtc-template>

	/***
	 *
	 * The initialize action (on CREATED->ALIVE transition)
	 *
	 * @return RTC::ReturnCode_t
	 *
	 *
	 */
	virtual RTC::ReturnCode_t onInitialize();

	/***
	 *
	 * The finalize action (on ALIVE->END transition)
	 *
	 * @return RTC::ReturnCode_t
	 *
	 *
	 */
	 // virtual RTC::ReturnCode_t onFinalize();

	 /***
	  *
	  * The startup action when ExecutionContext startup
	  *
	  * @param ec_id target ExecutionContext Id
	  *
	  * @return RTC::ReturnCode_t
	  *
	  *
	  */
	  // virtual RTC::ReturnCode_t onStartup(RTC::UniqueId ec_id);

	  /***
	   *
	   * The shutdown action when ExecutionContext stop
	   *
	   * @param ec_id target ExecutionContext Id
	   *
	   * @return RTC::ReturnCode_t
	   *
	   *
	   */
	   // virtual RTC::ReturnCode_t onShutdown(RTC::UniqueId ec_id);

	   /***
		*
		* The activated action (Active state entry action)
		*
		* @param ec_id target ExecutionContext Id
		*
		* @return RTC::ReturnCode_t
		*
		*
		*/
	virtual RTC::ReturnCode_t onActivated(RTC::UniqueId ec_id);

	/***
	 *
	 * The deactivated action (Active state exit action)
	 *
	 * @param ec_id target ExecutionContext Id
	 *
	 * @return RTC::ReturnCode_t
	 *
	 *
	 */
	virtual RTC::ReturnCode_t onDeactivated(RTC::UniqueId ec_id);

	/***
	 *
	 * The execution action that is invoked periodically
	 *
	 * @param ec_id target ExecutionContext Id
	 *
	 * @return RTC::ReturnCode_t
	 *
	 *
	 */
	virtual RTC::ReturnCode_t onExecute(RTC::UniqueId ec_id);

	/***
	 *
	 * The aborting action when main logic error occurred.
	 *
	 * @param ec_id target ExecutionContext Id
	 *
	 * @return RTC::ReturnCode_t
	 *
	 *
	 */
	 // virtual RTC::ReturnCode_t onAborting(RTC::UniqueId ec_id);

	 /***
	  *
	  * The error action in ERROR state
	  *
	  * @param ec_id target ExecutionContext Id
	  *
	  * @return RTC::ReturnCode_t
	  *
	  *
	  */
	  // virtual RTC::ReturnCode_t onError(RTC::UniqueId ec_id);

	  /***
	   *
	   * The reset action that is invoked resetting
	   *
	   * @param ec_id target ExecutionContext Id
	   *
	   * @return RTC::ReturnCode_t
	   *
	   *
	   */
	   // virtual RTC::ReturnCode_t onReset(RTC::UniqueId ec_id);

	   /***
		*
		* The state update action that is invoked after onExecute() action
		*
		* @param ec_id target ExecutionContext Id
		*
		* @return RTC::ReturnCode_t
		*
		*
		*/
		// virtual RTC::ReturnCode_t onStateUpdate(RTC::UniqueId ec_id);

		/***
		 *
		 * The action that is invoked when execution context's rate is changed
		 *
		 * @param ec_id target ExecutionContext Id
		 *
		 * @return RTC::ReturnCode_t
		 *
		 *
		 */
		 // virtual RTC::ReturnCode_t onRateChanged(RTC::UniqueId ec_id);


protected:
	// <rtc-template block="protected_attribute">

	// </rtc-template>

	// <rtc-template block="protected_operation">

	// </rtc-template>

	// Configuration variable declaration
	// <rtc-template block="config_declare">

	// </rtc-template>

	// DataInPort declaration
	// <rtc-template block="inport_declare">
	RTC::TimedBoolean m_touch;
	/*!
	 */
	RTC::InPort<RTC::TimedBoolean> m_touchIn;
	RTC::TimedDouble m_load;
	/*!
	 */
	RTC::InPort<RTC::TimedDouble> m_loadIn;
	RTC::TimedBoolean m_emg;
	/*!
	 */
	RTC::InPort<RTC::TimedBoolean> m_emgIn;

	// </rtc-template>


	// DataOutPort declaration
	// <rtc-template block="outport_declare">
	RTC::TimedDoubleSeq m_posload;
	/*!
	 */
	RTC::OutPort<RTC::TimedDoubleSeq> m_posloadOut;
	RTC::TimedBoolean m_BLTon;
	/*!
	 */
	RTC::OutPort<RTC::TimedBoolean> m_BLTonOut;
	RTC::TimedBoolean m_reoffset;
	/*!
	 */
	RTC::OutPort<RTC::TimedBoolean> m_reoffsetOut;

	// </rtc-template>

	// CORBA Port declaration
	// <rtc-template block="corbaport_declare">

	// </rtc-template>

	// Service declaration
	// <rtc-template block="service_declare">

	// </rtc-template>

	// Consumer declaration
	// <rtc-template block="consumer_declare">

	// </rtc-template>

private:
	// <rtc-template block="private_attribute">

	// </rtc-template>

	// <rtc-template block="private_operation">

	// </rtc-template>

};


extern "C"
{
	DLL_EXPORT void PressDriverInit(RTC::Manager* manager);
};

#endif // PRESSDRIVER_H
