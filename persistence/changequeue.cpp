#include "persistence/changequeue.h"

#include <algorithm>

namespace persistence
{
  void ChangeQueue::addStream(std::shared_ptr<DataStream> dataStream) { _dataStreams.push_back(std::move(dataStream)); }

  bool ChangeQueue::hasUninitializedStreams() const
  {
    return std::any_of(_dataStreams.begin(), _dataStreams.end(),
                       [](const std::shared_ptr<DataStream>& stream) { return !stream->isInitialized(); });
  }

  void ChangeQueue::applyStreamChanges()
  {
    // Remove invalid streams (streams which no longer have a listener)
    _dataStreams.erase(
        std::remove_if(_dataStreams.begin(), _dataStreams.end(), [](auto& stream) { return !stream->isValid(); }),
        _dataStreams.end());

    // Apply all of the strema changes
    std::vector<DataStreamDifferential> changes;
    std::unique_lock<std::mutex> lock(_streamChangesMutex);
    std::swap(_changeList.streamChanges, changes);
    lock.unlock();

    for (auto& change : changes)
    {
      auto it = std::find_if(_dataStreams.begin(), _dataStreams.end(),
                             [&](std::shared_ptr<DataStream>& ds) { return ds->streamId() == change.streamId; });
      if (it != _dataStreams.end())
        (*it)->applyChange(change.change);
    }
  }

  void ChangeQueue::addChanges(ChangeList list)
  {
    if (!list.streamChanges.empty())
    {
      std::unique_lock<std::mutex> lock(_streamChangesMutex);
      std::move(list.streamChanges.begin(), list.streamChanges.end(), std::back_inserter(_changeList.streamChanges));
      lock.unlock();
      _streamChangesAvailableSignal();
    }
  }

  void ChangeQueue::addStreamChange(int streamId, DataStreamChange change)
  {
    std::unique_lock<std::mutex> lock(_streamChangesMutex);
    _changeList.streamChanges.push_back({streamId, std::move(change)});
    lock.unlock();
    _streamChangesAvailableSignal();
  }

  boost::signals2::connection ChangeQueue::connectToStreamChangesAvailableSignal(boost::signals2::slot<void()> slot)
  {
    return _streamChangesAvailableSignal.connect(slot);
  }

} // namespace persistence
