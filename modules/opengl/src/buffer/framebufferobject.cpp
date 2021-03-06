/*********************************************************************************
 *
 * Inviwo - Interactive Visualization Workshop
 *
 * Copyright (c) 2012-2020 Inviwo Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *********************************************************************************/

#include <modules/opengl/buffer/framebufferobject.h>
#include <inviwo/core/util/assertion.h>

#include <fmt/format.h>
#include <string_view>

namespace inviwo {

using namespace std::literals;

inline void checkContext(std::string_view error, Canvas::ContextID org, SourceLocation loc) {
    if constexpr (cfg::assertions) {
        auto rc = RenderContext::getPtr();
        Canvas::ContextID curr = rc->activeContext();
        if (org != curr) {

            const auto message =
                fmt::format("{}: '{}' ({}) than it was created: '{}' ({})", error,
                            rc->getContextName(curr), curr, rc->getContextName(org), org);

            assertion(loc.getFile(), loc.getFunction(), loc.getLine(), message);
        }
    }
}

const std::array<GLenum, 16> FrameBufferObject::colorAttachmentEnums_ = {
    GL_COLOR_ATTACHMENT0,  GL_COLOR_ATTACHMENT1,  GL_COLOR_ATTACHMENT2,  GL_COLOR_ATTACHMENT3,
    GL_COLOR_ATTACHMENT4,  GL_COLOR_ATTACHMENT5,  GL_COLOR_ATTACHMENT6,  GL_COLOR_ATTACHMENT7,
    GL_COLOR_ATTACHMENT8,  GL_COLOR_ATTACHMENT9,  GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11,
    GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14, GL_COLOR_ATTACHMENT15};

FrameBufferObject::FrameBufferObject()
    : id_(0u)
    , attachedDepthId_(0)
    , attachedStencilId_(0)
    , attachedColorIds_{}
    , maxColorattachments_(0)
    , prevFbo_(0u)
    , prevDrawFbo_(0u)
    , prevReadFbo_(0u) {

    creationContext_ = RenderContext::getPtr()->activeContext();
    IVW_ASSERT(creationContext_, "A OpenGL Context has to be active");

    glGenFramebuffers(1, &id_);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorattachments_);

    drawBuffers_.reserve(maxColorattachments_);
    attachedColorIds_.resize(maxColorattachments_, 0);
}

FrameBufferObject::FrameBufferObject(FrameBufferObject&& rhs) noexcept
    : id_(rhs.id_)
    , attachedDepthId_{rhs.attachedDepthId_}
    , attachedStencilId_{rhs.attachedStencilId_}
    , attachedColorIds_{std::move(rhs.attachedColorIds_)}
    , drawBuffers_{std::move(rhs.drawBuffers_)}
    , maxColorattachments_{rhs.maxColorattachments_}
    , prevFbo_{rhs.prevFbo_}
    , prevDrawFbo_{rhs.prevDrawFbo_}
    , prevReadFbo_{rhs.prevReadFbo_}
    , creationContext_{rhs.creationContext_} {
    rhs.id_ = 0;
    rhs.prevFbo_ = 0;
    rhs.prevDrawFbo_ = 0;
    rhs.prevReadFbo_ = 0;
}

FrameBufferObject& FrameBufferObject::operator=(FrameBufferObject&& rhs) noexcept {
    if (this != &rhs) {
        id_ = rhs.id_;
        attachedDepthId_ = rhs.attachedDepthId_;
        attachedStencilId_ = rhs.attachedStencilId_;
        attachedColorIds_ = std::move(rhs.attachedColorIds_);
        drawBuffers_ = std::move(rhs.drawBuffers_);
        maxColorattachments_ = rhs.maxColorattachments_;
        prevFbo_ = rhs.prevFbo_;
        prevDrawFbo_ = rhs.prevDrawFbo_;
        prevReadFbo_ = rhs.prevReadFbo_;
        creationContext_ = rhs.creationContext_;

        rhs.id_ = 0;
        rhs.prevFbo_ = 0;
        rhs.prevDrawFbo_ = 0;
        rhs.prevReadFbo_ = 0;
    }
    return *this;
}

FrameBufferObject::~FrameBufferObject() {
    if (id_ != 0) {
        checkContext("FBO deleted in a different context"sv, creationContext_, IVW_SOURCE_LOCATION);
        deactivate();
        glDeleteFramebuffers(1, &id_);
    }
}

void FrameBufferObject::activate() {
    GLint currentFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);

    if (static_cast<GLint>(id_) != currentFbo) {
        // store currently bound FBO
        prevFbo_ = currentFbo;

        checkContext("FBO activated in a different context"sv, creationContext_,
                     IVW_SOURCE_LOCATION);

        glBindFramebuffer(GL_FRAMEBUFFER, id_);
        LGL_ERROR_CLASS;
    }
}

void FrameBufferObject::defineDrawBuffers() {
    // TODO: how to handle empty drawBuffers_ ? Do nothing or activate GL_COLOR_ATTACHMENT0 ?
    if (drawBuffers_.empty()) return;
    glDrawBuffers(static_cast<GLsizei>(drawBuffers_.size()), drawBuffers_.data());
    LGL_ERROR_CLASS;
}

void FrameBufferObject::deactivate() {
    if ((static_cast<GLuint>(prevFbo_) != id_) && isActive()) {
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo_);
        LGL_ERROR_CLASS;
    }
}

bool FrameBufferObject::isActive() const {
    checkContext("FBO used in a different context"sv, creationContext_, IVW_SOURCE_LOCATION);
    GLint currentFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFbo);
    return (static_cast<GLint>(id_) == currentFbo);
}

void FrameBufferObject::deactivateFBO() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

/******************************* 2D Texture *****************************************/

void FrameBufferObject::attachTexture(Texture2D* texture, GLenum attachmentID) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    performAttachTexture(attachmentID, texture->getID());
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentID, GL_TEXTURE_2D, texture->getID(), 0);
}

GLenum FrameBufferObject::attachColorTexture(Texture2D* texture) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID());
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentID, GL_TEXTURE_2D, texture->getID(), 0);
    return attachmentID;
}

GLenum FrameBufferObject::attachColorTexture(Texture2D* texture, int attachmentNumber,
                                             bool attachFromRear, int forcedLocation) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID(), attachmentNumber,
                                                    attachFromRear, forcedLocation);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentID, GL_TEXTURE_2D, texture->getID(), 0);

    return attachmentID;
}

/******************************* 2D Array Texture *****************************************/

void FrameBufferObject::attachTexture(Texture2DArray* texture, GLenum attachmentID) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    performAttachTexture(attachmentID, texture->getID());
    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0);
}

GLenum FrameBufferObject::attachColorTexture(Texture2DArray* texture) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID());
    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0);
    return attachmentID;
}

GLenum FrameBufferObject::attachColorTexture(Texture2DArray* texture, int attachmentNumber,
                                             bool attachFromRear, int forcedLocation) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID(), attachmentNumber,
                                                    attachFromRear, forcedLocation);
    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0);
    return attachmentID;
}

void FrameBufferObject::attachTextureLayer(Texture2DArray* texture, GLenum attachmentID, int layer,
                                           int level) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    performAttachTexture(attachmentID, texture->getID());
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachmentID, texture->getID(), level, layer);
}

GLenum FrameBufferObject::attachColorTextureLayer(Texture2DArray* texture, int layer) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID());
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0, layer);
    return attachmentID;
}

GLenum FrameBufferObject::attachColorTextureLayer(Texture2DArray* texture, int attachmentNumber,
                                                  int layer, bool attachFromRear,
                                                  int forcedLocation) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID(), attachmentNumber,
                                                    attachFromRear, forcedLocation);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0, layer);

    return attachmentID;
}

void FrameBufferObject::attachTexture(Texture3D* texture, GLenum attachmentID) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    performAttachTexture(attachmentID, texture->getID());
    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0);
}

GLenum FrameBufferObject::attachColorTexture(Texture3D* texture) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID());
    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0);
    return attachmentID;
}

GLenum FrameBufferObject::attachColorTexture(Texture3D* texture, int attachmentNumber,
                                             bool attachFromRear, int forcedLocation) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID(), attachmentNumber,
                                                    attachFromRear, forcedLocation);
    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, texture->getID(), 0);

    return attachmentID;
}

void FrameBufferObject::attachTextureLayer(Texture3D* texture, GLenum attachmentID, int layer) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    performAttachTexture(attachmentID, texture->getID());
    glFramebufferTexture3D(GL_FRAMEBUFFER, attachmentID, GL_TEXTURE_3D, texture->getID(), 0, layer);
}

GLenum FrameBufferObject::attachColorTextureLayer(Texture3D* texture, int layer) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID());
    glFramebufferTexture3D(GL_FRAMEBUFFER, attachmentID, GL_TEXTURE_3D, texture->getID(), 0, layer);
    return attachmentID;
}

GLenum FrameBufferObject::attachColorTextureLayer(Texture3D* texture, int attachmentNumber,
                                                  int layer, bool attachFromRear,
                                                  int forcedLocation) {
    IVW_ASSERT(isActive(), "FBO not active when attaching texture");
    GLenum attachmentID = performAttachColorTexture(texture->getID(), attachmentNumber,
                                                    attachFromRear, forcedLocation);
    glFramebufferTexture3D(GL_FRAMEBUFFER, attachmentID, GL_TEXTURE_3D, texture->getID(), 0, layer);
    return attachmentID;
}

void FrameBufferObject::detachTexture(GLenum attachmentID) {
    IVW_ASSERT(isActive(), "FBO not active when detaching texture");
    if (attachmentID == GL_DEPTH_ATTACHMENT) {
        attachedDepthId_ = 0;
    } else if (attachmentID == GL_STENCIL_ATTACHMENT) {
        attachedStencilId_ = 0;
    } else if (attachmentID == GL_DEPTH_STENCIL_ATTACHMENT) {
        attachedDepthId_ = 0;
        attachedStencilId_ = 0;
    } else {
        // check for valid attachmentID
        validateAttachmentId(attachmentID);

        // check whether given ID is already attached
        // keep internal state consistent and remove color attachment from draw buffers
        util::erase_remove(drawBuffers_, attachmentID);

        // set attachment state to unused
        attachedColorIds_[attachmentID - colorAttachmentEnums_[0]] = 0;
    }

    glFramebufferTexture(GL_FRAMEBUFFER, attachmentID, 0, 0);
}

void FrameBufferObject::detachAllTextures() {
    IVW_ASSERT(isActive(), "FBO not active when detaching texture");

    attachedDepthId_ = 0;
    attachedStencilId_ = 0;
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, 0, 0);

    for (const auto& buffer : drawBuffers_) {
        glFramebufferTexture(GL_FRAMEBUFFER, buffer, 0, 0);
    }
    drawBuffers_.clear();
    std::fill(attachedColorIds_.begin(), attachedColorIds_.end(), 0);
}

unsigned int FrameBufferObject::getID() const { return id_; }

const GLenum* FrameBufferObject::getDrawBuffersDeprecated() const { return &drawBuffers_[0]; }

int FrameBufferObject::getMaxColorAttachments() const { return maxColorattachments_; }

bool FrameBufferObject::hasColorAttachment() const { return !drawBuffers_.empty(); }

bool FrameBufferObject::hasDepthAttachment() const { return attachedDepthId_ != 0; }

bool FrameBufferObject::hasStencilAttachment() const { return attachedStencilId_ != 0; }

void FrameBufferObject::checkStatus() {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:  // All OK
            break;

        case GL_FRAMEBUFFER_UNDEFINED:
            LogWarn("GL_FRAMEBUFFER_UNDEFINED");
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            LogWarn("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            LogWarn("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            LogWarn("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            LogWarn("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
            break;

        case GL_FRAMEBUFFER_UNSUPPORTED:
            LogWarn("GL_FRAMEBUFFER_UNSUPPORTED");
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            LogWarn("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");
            break;

        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            LogWarn("GL_FRAMEBUFFER_INCOMPLETE_FORMATS");
            break;

        default:
            LogWarn("Unknown error " << status);
            break;
    }
}

void FrameBufferObject::setRead_Blit(bool set) const {
    if (set) {  // store currently bound draw FBO
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo_);

        checkContext("FBO activated in a different context"sv, creationContext_,
                     IVW_SOURCE_LOCATION);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, id_);
    } else {
        GLint currentReadFbo;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentReadFbo);
        if (currentReadFbo == prevReadFbo_) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, prevFbo_);
        }
    }
}

void FrameBufferObject::setDraw_Blit(bool set) {
    if (set) {  // store currently bound draw FBO
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo_);

        checkContext("FBO activated in a different context"sv, creationContext_,
                     IVW_SOURCE_LOCATION);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id_);
    } else {
        GLint currentDrawFbo;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFbo);
        if (currentDrawFbo == prevDrawFbo_) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevFbo_);
        }
    }
}

void FrameBufferObject::performAttachTexture(GLenum attachmentID, GLuint texId) {
    if (attachmentID == GL_DEPTH_ATTACHMENT) {
        attachedDepthId_ = texId;
    } else if (attachmentID == GL_STENCIL_ATTACHMENT) {
        attachedStencilId_ = texId;
    } else if (attachmentID == GL_DEPTH_STENCIL_ATTACHMENT) {
        attachedDepthId_ = texId;
        attachedStencilId_ = texId;
    } else {
        validateAttachmentId(attachmentID);

        // check whether given ID is already attached
        if (!attachedColorIds_[attachmentID - colorAttachmentEnums_[0]]) {
            drawBuffers_.push_back(attachmentID);
            attachedColorIds_[attachmentID - colorAttachmentEnums_[0]] = texId;
        }
    }
}

GLenum FrameBufferObject::performAttachColorTexture(GLuint texId) {
    // identify first unused color attachment ID
    const auto itUnUsed =
        util::find_if(attachedColorIds_, [](const auto& used) { return used == 0; });
    if (itUnUsed == attachedColorIds_.end()) {
        throw OpenGLException("Maximum number of color attachments reached.", IVW_CONTEXT);
    }

    const auto attachmentID = static_cast<GLenum>(
        colorAttachmentEnums_[0] + std::distance(attachedColorIds_.begin(), itUnUsed));
    drawBuffers_.push_back(attachmentID);
    *itUnUsed = texId;
    return attachmentID;
}

void FrameBufferObject::validateAttachmentId(GLenum attachmentID) const {
    if ((attachmentID < colorAttachmentEnums_[0]) ||
        (attachmentID > colorAttachmentEnums_[maxColorattachments_ - 1])) {
        throw OpenGLException(fmt::format("Invalid attachment id: {}", attachmentID), IVW_CONTEXT);
    }
}

GLenum FrameBufferObject::performAttachColorTexture(GLuint texId, int attachmentNumber,
                                                    bool attachFromRear, int forcedLocation) {
    if (static_cast<long>(drawBuffers_.size()) == maxColorattachments_) {
        throw OpenGLException("Maximum number of color attachments reached.", IVW_CONTEXT);
    }
    if ((attachmentNumber < 0) || (attachmentNumber > maxColorattachments_ - 1)) {
        throw OpenGLException(fmt::format("Invalid attachment id: {}", attachmentNumber),
                              IVW_CONTEXT);
    }

    attachmentNumber =
        (attachFromRear ? maxColorattachments_ - attachmentNumber - 1 : attachmentNumber);
    GLenum attachmentID = colorAttachmentEnums_[attachmentNumber];
    if (attachedColorIds_[attachmentNumber] == 0) {
        // new attachment, not registered before
        attachedColorIds_[attachmentNumber] = texId;
        if ((forcedLocation < 0) || (forcedLocation > static_cast<int>(drawBuffers_.size()))) {
            // no or invalid forced location
            drawBuffers_.push_back(attachmentID);
        } else {
            // forced location, position attachment at given position in drawBuffers_
            drawBuffers_.insert(drawBuffers_.begin() + forcedLocation, attachmentID);
        }
    } else if ((forcedLocation > -1) && (forcedLocation < static_cast<int>(drawBuffers_.size()))) {
        // attachment is already registered, but buffer location is forced.
        // adjust position within drawBuffers_ only if required
        if (drawBuffers_[forcedLocation] != attachmentID) {
            drawBuffers_.erase(std::remove(drawBuffers_.begin(), drawBuffers_.end(), attachmentID),
                               drawBuffers_.end());
            drawBuffers_.insert(drawBuffers_.begin() + forcedLocation, attachmentID);
        }
    }

    return attachmentID;
}

int FrameBufferObject::getAttachmentLocation(GLenum attachmentID) const {
    if ((attachmentID == GL_DEPTH_ATTACHMENT) || (attachmentID == GL_STENCIL_ATTACHMENT) ||
        (attachmentID == GL_DEPTH_STENCIL_ATTACHMENT))
        return 0;

    auto it = util::find_if(drawBuffers_, [&](const auto& b) { return b == attachmentID; });
    if (it != drawBuffers_.end()) {
        return static_cast<int>(std::distance(drawBuffers_.begin(), it));
    } else {
        // given ID not attached
        return -1;
    }
}

std::string FrameBufferObject::printBuffers() const {
    std::stringstream str;
    if (drawBuffers_.empty()) {
        str << "none";
    } else {
        for (std::size_t i = 0; i < drawBuffers_.size(); ++i) {
            str << ((i != 0) ? ", " : "") << getAttachmentStr(drawBuffers_[i]);
        }
    }
    str << " / "
        << std::count_if(attachedColorIds_.begin(), attachedColorIds_.end(),
                         [](GLuint id) { return id != 0; })
        << " buffers active";

    return str.str();
}

std::string FrameBufferObject::getAttachmentStr(GLenum attachmentID) {
    if ((attachmentID < GL_COLOR_ATTACHMENT0) || (attachmentID > GL_COLOR_ATTACHMENT15))
        return "GL_NONE";

    std::stringstream str;
    str << "GL_COLOR_ATTACHMENT" << (attachmentID - colorAttachmentEnums_[0]);
    return str.str();
}

}  // namespace inviwo
