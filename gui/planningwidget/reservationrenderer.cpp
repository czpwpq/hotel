#include "gui/planningwidget/reservationrenderer.h"

#include "gui/planningwidget/context.h"
#include "gui/planningwidget/planningboardlayout.h"

namespace gui
{
  namespace planningwidget
  {

    QColor mix(const QColor& c1, const QColor& c2)
    {
      return QColor((c1.red() + c2.red() * 3) / 4, (c1.green() + c2.green() * 3) / 4, (c1.blue() + c2.blue() * 3) / 4,
                    (c1.alpha() + c2.alpha() * 3) / 4);
    }

    void ReservationRenderer::paintAtom(QPainter* painter, const Context& context,
                                        const hotel::Reservation& reservation, const hotel::ReservationAtom& atom,
                                        const QRectF& atomRect, bool isSelected) const
    {
      drawAtomBackground(painter, context, reservation, atom, atomRect, isSelected);
      drawAtomText(painter, context, reservation, atom, atomRect, isSelected);
    }

    void ReservationRenderer::paintReservationConnections(QPainter* painter, const Context& context,
                                                          const std::vector<QRectF>& atomRects, bool isSelected) const
    {
      if (isSelected)
      {
        // Draw the connection links between items
        auto& appearance = context.appearance();
        if (atomRects.size() > 1)
        {
          painter->save();
          painter->setRenderHint(QPainter::Antialiasing, true);
          painter->setPen(QPen(appearance.selectionColor, 2));

          for (auto i = 0; i < static_cast<int>(atomRects.size()) - 1; ++i)
          {
            auto previousBox = atomRects[i];
            auto nextBox = atomRects[i + 1];

            // Calculate the two points
            auto x1 = previousBox.right();
            auto y1 = previousBox.top() + previousBox.height() / 2;
            auto x2 = nextBox.left() + 1;
            auto y2 = nextBox.top() + nextBox.height() / 2;

            // Draw the two rectangular handles
            const int handleSize = appearance.atomConnectionHandleSize;
            const int linkOverhang = appearance.atomConnectionOverhang;
            const QColor& handleColor = appearance.selectionColor;
            painter->fillRect(QRect(x1, y1 - handleSize, handleSize, handleSize * 2), handleColor);
            painter->fillRect(QRect(x2 - handleSize, y2 - handleSize, handleSize, handleSize * 2), handleColor);

            // Draw the zig-yag line between handles
            QPointF points[] = {QPoint(x1, y1), QPoint(x1 + linkOverhang, y1), QPoint(x2 - linkOverhang, y2),
                                QPoint(x2, y2)};
            painter->drawPolyline(points, 4);
          }

          painter->restore();
        }
      }
    }

    void ReservationRenderer::drawAtomBackground(QPainter* painter, const Context& context,
                                                 const hotel::Reservation& reservation,
                                                 const hotel::ReservationAtom& atom, const QRectF& atomRect,
                                                 bool isSelected) const
    {
      auto itemColor = getAtomBackgroundColor(context, reservation, atom, isSelected);
      const int cornerRadius = 5;

      painter->save();
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setBrush(itemColor);
      painter->setPen(itemColor.darker(200));

      // Construct a rounded rectangle. Not all of the corners are rounded. Only the parts corresponding to the
      // beginning
      // or end of a reservation are rounded
      auto borderRect = atomRect.adjusted(1, 1, 0, -1).adjusted(-0.5, -0.5, -0.5, -0.5);
      QPainterPath path;
      path.setFillRule(Qt::WindingFill);
      path.addRoundedRect(borderRect, cornerRadius, cornerRadius, Qt::AbsoluteSize);
      if (&atom != reservation.firstAtom())
        path.addRect(borderRect.adjusted(0, 0, -cornerRadius, 0));
      if (&atom != reservation.lastAtom())
        path.addRect(borderRect.adjusted(cornerRadius, 0, 0, 0));
      painter->drawPath(path.simplified());
      painter->restore();
    }

    void ReservationRenderer::drawAtomText(QPainter* painter, const Context& context,
                                           const hotel::Reservation& reservation, const hotel::ReservationAtom& atom,
                                           const QRectF& atomRect, bool isSelected) const
    {
      painter->save();
      painter->setClipRect(atomRect);
      painter->setPen(getAtomTextColor(context, reservation, atom, isSelected));
      painter->setFont(context.appearance().atomTextFont);
      painter->drawText(atomRect.adjusted(5, 2, -2, -2), Qt::AlignLeft | Qt::AlignVCenter,
                        getAtomText(reservation, atom, isSelected));
      painter->restore();
    }

    QColor ReservationRenderer::getAtomBackgroundColor(const Context& context, const hotel::Reservation& reservation,
                                                       [[maybe_unused]] const hotel::ReservationAtom& atom,
                                                       bool isSelected) const
    {
      using ReservationStatus = hotel::Reservation::ReservationStatus;
      auto& appearance = context.appearance();
      auto status = reservation.status();

      if (status == ReservationStatus::Temporary)
        return appearance.atomTemporaryColor;

      auto color = appearance.atomDefaultColor;

      if (isSelected)
      {
        if (status == ReservationStatus::CheckedOut || status == ReservationStatus::Archived)
          color = appearance.atomArchivedSelectedColor;
        else
          color = appearance.atomSelectedColor;
      }
      else
      {
        if (status == ReservationStatus::Archived)
          color = appearance.atomArchivedColor;
        else if (status == ReservationStatus::CheckedOut)
          color = appearance.atomCheckedOutColor;
        else if (status == ReservationStatus::CheckedIn)
          color = appearance.atomCheckedInColor;
        else if (status == ReservationStatus::New)
          color = appearance.atomUnconfirmedColor;
      }

      color.setAlpha(220);
      return color;
    }

    QColor ReservationRenderer::getAtomTextColor(const Context& context, const hotel::Reservation& reservation,
                                                 const hotel::ReservationAtom& atom, bool isSelected) const
    {
      auto& appearance = context.appearance();
      auto itemColor = getAtomBackgroundColor(context, reservation, atom, isSelected);
      if (itemColor.lightness() > 200)
        return appearance.atomDarkTextColor;
      else
        return appearance.atomLightTextColor;
    }

    QString ReservationRenderer::getAtomText(const hotel::Reservation& reservation, const hotel::ReservationAtom& atom,
                                             [[maybe_unused]] bool isSelected) const
    {
      using ReservationStatus = hotel::Reservation::ReservationStatus;

      auto description = QString::fromStdString(reservation.description());
      QString text;

      if (reservation.status() == ReservationStatus::Temporary)
        text = QString("%1 (%2)").arg(description).arg(reservation.length());
      else
      {
        text = QString("%1+%2 %3 (%4)")
                   .arg(reservation.numberOfAdults())
                   .arg(reservation.numberOfChildren())
                   .arg(description)
                   .arg(reservation.length());
        if (&atom != reservation.firstAtom())
          text = "\xE2\x96\xB8 " + text;
      }

      return text;
    }

    void PrivacyReservationRenderer::paintAtom(QPainter* painter, const Context& context,
                                               const hotel::Reservation& reservation,
                                               const hotel::ReservationAtom& atom, const QRectF& atomRect,
                                               bool isSelected) const
    {
      drawAtomBackground(painter, context, reservation, atom, atomRect, isSelected);
      if (isSelected)
        drawAtomText(painter, context, reservation, atom, atomRect, isSelected);
    }

    QColor PrivacyReservationRenderer::getAtomBackgroundColor(const Context& context,
                                                              [[maybe_unused]] const hotel::Reservation& reservation,
                                                              [[maybe_unused]] const hotel::ReservationAtom& atom,
                                                              bool isSelected) const
    {
      auto& appearance = context.appearance();
      return isSelected ? appearance.atomSelectedColor : appearance.atomDefaultColor;
    }

    void PrivacyReservationRenderer::drawAtomBackground(QPainter* painter, const Context& context,
                                                        const hotel::Reservation& reservation,
                                                        const hotel::ReservationAtom& atom, const QRectF& atomRect,
                                                        bool isSelected) const
    {
      QColor backgroundColor = getAtomBackgroundColor(context, reservation, atom, isSelected);
      painter->fillRect(atomRect.adjusted(0, 0, 0, -1), backgroundColor);
    }

    QColor HighlightArrivalsRenderer::getAtomBackgroundColor(const Context& context,
                                                             const hotel::Reservation& reservation,
                                                             const hotel::ReservationAtom& atom, bool isSelected) const
    {
      const auto baseColor = ReservationRenderer::getAtomBackgroundColor(context, reservation, atom, isSelected);
      const auto mutedColorV = baseColor.red() / 3 + baseColor.green() / 3 + baseColor.blue() / 3;
      const auto mutedColor = mix(baseColor, QColor(mutedColorV, mutedColorV, mutedColorV));

      bool isHighlighted = atom.dateRange().begin() == context.layout().pivotDate();
      if (isHighlighted)
        return baseColor;
      if (isSelected)
        return context.appearance().atomSelectedColor;

      return mutedColor;
    }

  } // namespace planningwidget
} // namespace gui
