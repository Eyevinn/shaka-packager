// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/text_chunker.h>

#include <absl/log/check.h>

#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
}  // namespace

TextChunker::TextChunker(double segment_duration_in_seconds,
                         int64_t start_segment_number)
    : segment_duration_in_seconds_(segment_duration_in_seconds),
      segment_number_(start_segment_number){};

Status TextChunker::Process(std::unique_ptr<StreamData> data) {
  switch (data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(data->stream_info));
    case StreamDataType::kTextSample:
      return OnTextSample(data->text_sample);
    case StreamDataType::kCueEvent:
      return OnCueEvent(data->cue_event);
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type for this handler");
  }
}

Status TextChunker::OnFlushRequest(size_t /*input_stream_index*/) {
  // Keep outputting segments until all the samples leave the system. Calling
  // |DispatchSegment| will remove samples over time.
  while (samples_in_current_segment_.size()) {
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  return FlushAllDownstreams();
}

Status TextChunker::OnStreamInfo(std::shared_ptr<const StreamInfo> info) {
  time_scale_ = info->time_scale();
  segment_duration_ = ScaleTime(segment_duration_in_seconds_);

  return DispatchStreamInfo(kStreamIndex, std::move(info));
}

Status TextChunker::OnCueEvent(std::shared_ptr<const CueEvent> event) {
  // We are going to end the current segment prematurely using the cue event's
  // time as the new segment end.

  // Because the cue should have been inserted into the stream such that no
  // later sample could start before it does, we know that there should
  // be no later samples starting before the cue event.

  // Convert the event's time to be scaled to the time of each sample.
  const int64_t event_time = ScaleTime(event->time_in_seconds);
  // Output all full segments before the segment that the cue event interupts.
  while (segment_start_ + segment_duration_ < event_time) {
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  const int64_t shorten_duration = event_time - segment_start_;
  RETURN_IF_ERROR(DispatchSegment(shorten_duration));
  return DispatchCueEvent(kStreamIndex, std::move(event));
}

Status TextChunker::OnTextSample(std::shared_ptr<const TextSample> sample) {
  // Output all segments that come before our new sample.
  const int64_t sample_start = sample->start_time();
  const auto role = sample->role();

  switch (role) {
    case TextSampleRole::kCue:
      LOG(INFO) << "Cue sample started. pts=" << sample->start_time();
      break;
    case TextSampleRole::kCueEnd:
      LOG(INFO) << "CueEnd sample. pts=" << sample->EndTime();
      break;
    case TextSampleRole::kMediaHeartBeat:
      LOG(INFO) << "Media HeartBeat. pts=" << sample->start_time();
      break;
    case TextSampleRole::kTextHeartBeat:
      LOG(INFO) << "Text HeartBeat. pts=" << sample->start_time();
      break;
    default:
      LOG(ERROR) << "Unknown role encountered. pts=" << sample->start_time();
  }

  // If we have not seen a sample yet, base all segments off the first sample's
  // start time.
  if (segment_start_ < 0) {
    // Force the first segment to start at the segment that would have started
    // before the sample. This should allow segments from different streams to
    // align.
    segment_start_ = (sample_start / segment_duration_) * segment_duration_;
    LOG(INFO) << "first segment start=" << segment_start_;
  }

  if (role == TextSampleRole::kCueEnd) {
    for (auto s : samples_in_current_segment_) {
      if (s->role() == TextSampleRole::kCue && s->sub_stream_index() == sample->sub_stream_index()) {
        auto end_time = sample->EndTime();
        auto new_dur = end_time - s->start_time();
        s->set_duration(new_dur);
        LOG(INFO) << "reset sample duration pts=" << s->start_time() << " dur=" << new_dur;
      }
    }
  }

  // We need to write all the segments that would have ended before the new
  // sample started.
  while (sample_start >= segment_start_ + segment_duration_) {
    // |DispatchSegment| will advance |segment_start_|.
    RETURN_IF_ERROR(DispatchSegment(segment_duration_));
  }

  if (sample->role() == TextSampleRole::kCue) {
    // auto nonconst_sample = std::const_pointer_cast<TextSample>(sample);
    auto sample_copy = std::make_shared<TextSample>(*sample);
    samples_in_current_segment_.push_back(std::move(sample_copy));
  } else {
    sample.reset();  // Free sample
  }

  return Status::OK;
}

Status TextChunker::DispatchSegment(int64_t duration) {
  DCHECK_GT(duration, 0) << "Segment duration should always be positive";

  // Output all the samples that are part of the segment.
  for (const auto& sample : samples_in_current_segment_) {
    LOG(WARNING) << "DispatchTextSample, pts=" << sample->start_time() << " end=" << sample->EndTime();
    RETURN_IF_ERROR(DispatchTextSample(kStreamIndex, sample));
  }

  // Output the segment info.
  std::shared_ptr<SegmentInfo> info = std::make_shared<SegmentInfo>();
  info->start_timestamp = segment_start_;
  info->duration = duration;
  info->segment_number = segment_number_++;

  LOG(INFO) << "DispatchSegmentInfo nr=" << info->segment_number << " start=" << segment_start_ << " end=" << segment_start_ + duration;
  RETURN_IF_ERROR(DispatchSegmentInfo(kStreamIndex, std::move(info)));

  // Move onto the next segment.
  const int64_t new_segment_start = segment_start_ + duration;
  segment_start_ = new_segment_start;
  LOG(INFO) << "New segment start=" << segment_start_ << " end=" << segment_start_ + duration;

  // Remove all samples that end before the (new) current segment started.
  samples_in_current_segment_.remove_if(
      [new_segment_start](const std::shared_ptr<const TextSample>& sample) {
        // For the sample to even be in this list, it should have started
        // before the (new) current segment.
        DCHECK_LT(sample->start_time(), new_segment_start);
        auto should_remove = sample->EndTime() <= new_segment_start;
        LOG(INFO) << "sample start=" << sample->start_time() << " end=" << sample->EndTime() << " remove=" << should_remove;
        return sample->EndTime() <= new_segment_start;
      });

  return Status::OK;
}

int64_t TextChunker::ScaleTime(double seconds) const {
  DCHECK_GT(time_scale_, 0) << "Need positive time scale to scale time.";
  return static_cast<int64_t>(seconds * time_scale_);
}
}  // namespace media
}  // namespace shaka
