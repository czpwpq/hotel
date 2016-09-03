#include "hotel/reservation.h"

namespace hotel {

hotel::Reservation::Reservation(const std::string &description, int roomId, boost::gregorian::date_period dateRange)
  : _description(description)
{
  _atoms.push_back(std::make_unique<ReservationAtom>(this, roomId, dateRange));
}

ReservationAtom *Reservation::addContinuation(int room, boost::gregorian::date date)
{
  auto& lastAtom = *_atoms.rbegin();

  if (lastAtom->_dateRange.end() >= date)
  {
    std::cerr << "Cannot create reservation continuation... The given date preceeds the end date of the last atom" << std::endl;
    return nullptr;
  }

  _atoms.push_back(std::make_unique<ReservationAtom>(this, room, boost::gregorian::date_period(lastAtom->_dateRange.end(), date)));
}

const int Reservation::length() const
{
  return (_atoms[_atoms.size()-1]->_dateRange.end() - _atoms[0]->_dateRange.begin()).days();
}

} // namespace hotel
