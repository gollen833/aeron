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
package io.aeron.driver;

import io.aeron.protocol.StatusMessageFlyweight;

import java.net.InetSocketAddress;

import static io.aeron.logbuffer.LogBufferDescriptor.computePosition;

/**
 * Default unicast sender flow control strategy.
 * <p>
 * Max of right edges.
 * No tracking of receivers.
 */
public class UnicastFlowControl implements FlowControl
{
    private long lastPosition = 0;

    /**
     * {@inheritDoc}
     */
    public long onStatusMessage(
        final StatusMessageFlyweight flyweight,
        final InetSocketAddress receiverAddress,
        final long senderLimit,
        final int initialTermId,
        final int positionBitsToShift,
        final long timeNs)
    {
        final long position = computePosition(
            flyweight.consumptionTermId(),
            flyweight.consumptionTermOffset(),
            positionBitsToShift,
            initialTermId);

        lastPosition = Math.max(lastPosition, position);

        return Math.max(senderLimit, position + flyweight.receiverWindowLength());
    }

    /**
     * {@inheritDoc}
     */
    public void initialize(final int initialTermId, final int termBufferLength)
    {
    }

    /**
     * {@inheritDoc}
     */
    public long onIdle(final long timeNs, final long senderLimit, final long senderPosition, final boolean isEos)
    {
        return senderLimit;
    }
}
