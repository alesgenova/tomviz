/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizReaction_h
#define tomvizReaction_h

#include "pipeline/PortType.h"

#include <pqReaction.h>

namespace tomviz {

class Reaction : public pqReaction
{
  Q_OBJECT

public:
  Reaction(QAction* parent);

  bool eventFilter(QObject* obj, QEvent* event) override;

protected:
  void updateEnableState() override;

  /// Set the port types this reaction's transform accepts. Defaults to
  /// ImageData (the supertype, compatible with Volume and TiltSeries).
  void setAcceptedInputTypes(pipeline::PortTypes types);

private:
  pipeline::PortTypes m_acceptedInputTypes = pipeline::PortType::ImageData;
  bool m_ctrlHeld = false;
};
} // namespace tomviz
#endif
