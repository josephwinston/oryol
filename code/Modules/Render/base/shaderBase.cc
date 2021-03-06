//------------------------------------------------------------------------------
//  shaderBase.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "shaderBase.h"

namespace Oryol {
namespace Render {
    
//------------------------------------------------------------------------------
shaderBase::shaderBase() :
type(ShaderType::InvalidShaderType) {
    // empty
}

//------------------------------------------------------------------------------
ShaderType::Code
shaderBase::GetShaderType() const {
    return this->type;
}

//------------------------------------------------------------------------------
void
shaderBase::clear() {
    this->type = ShaderType::InvalidShaderType;
}

//------------------------------------------------------------------------------
void
shaderBase::setShaderType(ShaderType::Code t) {
    this->type = t;
}

} // namespace Render
} // namespace Oryol