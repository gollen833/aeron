/*
 * Copyright 2014-2019 Real Logic Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package io.aeron.archive;

import io.aeron.archive.codecs.SourceLocation;

/**
 * Listener to control request messages from archive clients.
 */
interface ControlRequestListener
{
    void onConnect(long correlationId, int streamId, int version, String channel);

    void onCloseSession(long controlSessionId);

    void onStartRecording(
        long controlSessionId,
        long correlationId,
        int streamId,
        String channel,
        SourceLocation sourceLocation);

    void onStopRecording(long controlSessionId, long correlationId, int streamId, String channel);

    void onStartReplay(
        long controlSessionId,
        long correlationId,
        long recordingId,
        long position,
        long length,
        int replayStreamId,
        String replayChannel);

    void onBoundedStartReplay(
        long controlSessionId,
        long correlationId,
        long recordingId,
        long position,
        long length,
        int boundCounterId,
        int replayStreamId,
        String replayChannel);

    void onListRecordings(long controlSessionId, long correlationId, long fromRecordingId, int recordCount);

    void onListRecordingsForUri(
        long controlSessionId,
        long correlationId,
        long fromRecordingId,
        int recordCount,
        int streamId,
        byte[] channelFragment);

    void onListRecording(long controlSessionId, long correlationId, long recordingId);

    void onStopReplay(long controlSessionId, long correlationId, long replaySessionId);

    void onStopAllReplays(long controlSessionId, long correlationId, long recordingId);

    void onExtendRecording(
        long controlSessionId,
        long correlationId,
        long recordingId,
        int streamId,
        String channel,
        SourceLocation sourceLocation);

    void onGetRecordingPosition(long controlSessionId, long correlationId, long recordingId);

    void onTruncateRecording(long controlSessionId, long correlationId, long recordingId, long position);

    void onStopRecordingSubscription(long controlSessionId, long correlationId, long subscriptionId);

    void onGetStopPosition(long controlSessionId, long correlationId, long recordingId);

    void onFindLastMatchingRecording(
        long controlSessionId,
        long correlationId,
        long minRecordingId,
        int sessionId,
        int streamId,
        byte[] channelFragment);

    void onListRecordingSubscriptions(
        long controlSessionId,
        long correlationId,
        int pseudoIndex,
        int subscriptionCount,
        boolean applyStreamId,
        int streamId,
        String channelFragment);
}
