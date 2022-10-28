#include <stdafx.h>
#include <boost/filesystem.hpp>
#include "NVRControlModule.h"
#include "Mp4WriterSink.h"
#include "Module.h"
#include "Command.h"

class NVRControlModule::Detail
{
public:
    Detail(NVRControlModuleProps& _props) : mProps(_props)
    {
    }

    ~Detail()
    {
    }
    void setProps(NVRControlModuleProps _props)
    {
        mProps = _props;
    }
    NVRControlModuleProps mProps;
};


NVRControlModule::NVRControlModule(NVRControlModuleProps _props)
    :AbsControlModule(_props)
{
    mDetail.reset(new Detail(_props));
}

NVRControlModule::~NVRControlModule() {}

bool NVRControlModule::validateInputPins()
{
    return true;
}

bool NVRControlModule::validateOutputPins()
{
    return true;
}

bool NVRControlModule::validateInputOutputPins()
{
    return true;
}

bool NVRControlModule::handleCommand(Command::CommandType type, frame_sp& frame)
{
    if (type == Command::CommandType::NVRCommandRecord)
    {
        NVRCommandRecord cmd;
        getCommand(cmd, frame);
        auto RecordObj = RecordCommand(cmd.doRecording);
        for (int i = 0; i < pipelineModules.size(); i++)
        {
            if (pipelineModules[i]->getId() == "mp4WritersinkModule_4")
            {
                auto mp4Writer = reinterpret_pointer_cast<Mp4WriterSink>(pipelineModules[i]);
                bool isTaskDone = mp4Writer->stub(RecordObj);
            }
        }
        return true;
    }
    if (type == Command::CommandType::NVRCommandExport)
    {
        // mp4writer export command
        NVRCommandExport cmd;
        getCommand(cmd, frame);
        boost::shared_ptr<ExportCommand>ExportObj;
        // change to cmd.startExportTS
        ExportObj->startTime = cmd.startExport;
        ExportObj->stopTime = cmd.stopExport;
        for (int i = 0; i < pipelineModules.size(); i++)
        {
            if (pipelineModules[i]->getId() == "mp4WritersinkModule_4")
            {
                auto mp4Writer = reinterpret_pointer_cast<Mp4WriterSink>(pipelineModules[i]);
            }
        }
    }
    return Module::handleCommand(type, frame);
}

bool NVRControlModule::handlePropsChange(frame_sp& frame)
{
    NVRControlModuleProps props(mDetail->mProps);
    auto ret = Module::handlePropsChange(frame, props);
    mDetail->setProps(props);
    return ret;
}

bool NVRControlModule::init()
{
    if (!Module::init())
    {
        return false;
    }
    return true;
}

bool NVRControlModule::term()
{
    return Module::term();
}

NVRControlModuleProps NVRControlModule::getProps()
{
    fillProps(mDetail->mProps);
    return mDetail->mProps;
}

void NVRControlModule::setProps(NVRControlModuleProps& props)
{
    Module::addPropsToQueue(props);
}

bool NVRControlModule::record(bool record)
{
    NVRCommandRecord cmd;
    cmd.doRecording = record;
    return queueCommand(cmd);
}

bool NVRControlModule::export(uint64_t ts, uint64_t te)
{
    NVRCommandExport cmd;
    cmd.startExport = ts;
    cmd.stopExport = te;
    return queueCommand(cmd);
}